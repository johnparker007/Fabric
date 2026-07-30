#include "ProductionAmberAdapter.h"

#include "ProductionAmberAbi.h"
#include "AmberDynamicLibrary.h"
#include "AmberTrace.h"
#include "fabric/fabric_amber.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <set>
#include <sstream>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fabric {
namespace {
static_assert(alignof(PA2_OutputSnapshot) == 4,
              "production Amber snapshot must retain pack(4)");
static_assert(sizeof(PA2_OutputSnapshot) == 24812,
              "production Amber snapshot ABI size changed");
static_assert(sizeof(PA2_AudioFormat) == 24,
              "production Amber audio ABI size changed");
template <typename T>
T resolve(AmberDynamicLibrary &library, const char *name) {
  void *symbol = library.symbol(name);
  T result = nullptr;
  static_assert(sizeof(result) == sizeof(symbol), "function pointer size");
  std::memcpy(&result, &symbol, sizeof(result));
  return result;
}



class ProductionAmberInstance final : public FabricBackendInstance {
public:
  ProductionAmberInstance(std::unique_ptr<AmberDynamicLibrary> library, ProductionAmberApi api,
                 std::vector<std::string> program,
                 std::vector<std::string> sound,
                 const FabricAmberConfigurationV1 *configuration,
                 std::string path, FabricDiagnosticCallback diagnostic,
                 void *diagnostic_user_data)
      : library_(std::move(library)), api_(api), program_(std::move(program)),
        sound_(std::move(sound)), path_(std::move(path)),
        diagnostic_(diagnostic), diagnostic_user_data_(diagnostic_user_data) {
    if (configuration)
      config_ = *configuration;
  }
  ~ProductionAmberInstance() override {
    if (started_ && !stopped_)
      api_.Shutdown();
  }

  FabricResult initialise() noexcept override {
    emit("AmberInitialiseBegin", "result=pending");
    const uint8_t native = api_.Initialise();
    emit("AmberInitialiseEnd", "native_return=" + std::to_string(native) +
                                   "; result=" +
                                   (native ? "success" : "failure"));
    amber_trace::Write("Initialise: Amber return=" + std::to_string(native));
    if (!native)
      return fail("Initialise", "Amber return=0; DLL='" + path_ + "'");
    started_ = true;
    if (!load_roms(api_.LoadROM, program_, "program"))
      return FABRIC_NOT_FOUND;
    if (!sound_.empty() && !load_roms(api_.LoadSoundROM, sound_, "sound"))
      return api_.LoadSoundROM ? FABRIC_NOT_FOUND : FABRIC_NOT_SUPPORTED;
    /* The production core may clear all machine configuration in Reset.  Use
     * the one reset path for startup and every later reset so configuration
     * is never applied on the wrong side of that boundary. */
    const FabricResult reset_result = reset();
    if (reset_result != FABRIC_OK)
      return reset_result;
    amber_trace::Write("initialisation complete: Fabric result=0");
    return ok();
  }
  FabricResult reset() noexcept override {
    emit("AmberResetBegin", "result=pending");
    api_.Reset();
    audio_frames_available_ = 0;
    audio_frame_fraction_ = 0;
    emit("AmberResetEnd", "result=success");
    if (reset_trace_count_ < 8)
      amber_trace::Write("Reset: native reset completed");
    const FabricResult configured = apply_configuration("Reset configuration");
    if (configured != FABRIC_OK)
      return configured;
    if (reset_trace_count_ < 8) {
      amber_trace::Write("Reset: Fabric result=0");
      ++reset_trace_count_;
    }
    return ok();
  }
  FabricResult advance(uint64_t ns) noexcept override {
    constexpr uint64_t tick_ns = UINT64_C(1000000);
    constexpr uint32_t request = 8000;
    constexpr uint64_t maximum_catch_up = 3;
    const uint64_t whole_ticks = ns / tick_ns;
    const uint64_t combined_remainder = remainder_ + ns % tick_ns;
    const uint64_t available_ticks =
        whole_ticks + combined_remainder / tick_ns;
    remainder_ = combined_remainder % tick_ns;
    /* The direct pump clamps a delayed boundary to three calls and discards
     * excess whole ticks.  Only a sub-millisecond remainder is retained. */
    const uint64_t executed_ticks =
        std::min<uint64_t>(available_ticks, maximum_catch_up);
    for (uint64_t tick = 0; tick < executed_ticks; ++tick) {
      const int32_t native = api_.Run(request);
      if (audio_sample_rate_) {
        audio_frame_fraction_ += audio_sample_rate_;
        const uint64_t generated = audio_frame_fraction_ / 1000u;
        audio_frame_fraction_ %= 1000u;
        const uint64_t maximum_queue =
            (static_cast<uint64_t>(audio_sample_rate_) * maximum_catch_up +
             999u) /
            1000u;
        audio_frames_available_ = std::min<uint64_t>(
            maximum_queue, audio_frames_available_ + generated);
        if (audio_generate_diagnostic_count_ < 32) {
          emit("AudioBudget",
               "tick=" + std::to_string(native_tick_ + 1) +
                   "; earned_frames=" + std::to_string(generated) +
                   "; earned_samples=" +
                   std::to_string(generated * audio_channel_count_) +
                   "; queue_depth=" +
                   std::to_string(audio_frames_available_) +
                   "; result=success");
          ++audio_generate_diagnostic_count_;
        }
      }
      /* The flat ABI exposes the emulator/CPU return value for observation;
       * it does not define that value as a progress count or error status.
       * The requested argument is the consumed time budget. */
      if (advance_trace_count_ < 32) {
        emit("AmberRun", "tick=" + std::to_string(native_tick_ + 1) +
                             "; requested_cycles=" +
                             std::to_string(request) +
                             "; native_return=" + std::to_string(native) +
                             "; result=success");
        ++native_tick_;
        amber_trace::Write("Run: elapsed_ns=" + std::to_string(ns) +
                           "; requested_cycles=" + std::to_string(request) +
                           "; retained_ns=" + std::to_string(remainder_) +
                           "; native_return=" + std::to_string(native) +
                           "; Fabric result=0");
        ++advance_trace_count_;
      }
    }
    return ok();
  }
  FabricResult shutdown() noexcept override {
    release_inputs();
    if (stopped_)
      return shutdown_result_;
    stopped_ = true;
    if (started_ && !api_.Shutdown())
      return shutdown_result_ =
                 fail("Shutdown", "Amber return=0; DLL='" + path_ + "'");
    started_ = false;
    emit("AudioQueueShutdown",
         "discarded_frames=" + std::to_string(audio_frames_available_) +
             "; result=success");
    audio_frames_available_ = 0;
    emit("AmberShutdown", "native_return=1; result=success");
    amber_trace::Write("Shutdown: Amber return=1; Fabric result=0");
    return shutdown_result_ = ok();
  }
  FabricResult submit_input(const FabricInput &input) noexcept override {
    if (input.numerical_index < 0 || input.numerical_index > 255)
      return invalid(
          "production Amber switch index must be in the range 0..255");
    const uint8_t index = static_cast<uint8_t>(input.numerical_index);
    if (input.active) {
      api_.TurnSwitchOn(index);
      asserted_.insert(index);
    } else {
      api_.TurnSwitchOff(index);
      asserted_.erase(index);
    }
    return ok();
  }
  FabricResult capabilities(FabricCapabilities &out) noexcept override {
    out.flags = FABRIC_CAPABILITY_DIGITAL_INPUT | FABRIC_CAPABILITY_LAMPS |
                FABRIC_CAPABILITY_REELS | FABRIC_CAPABILITY_CHARACTER_DISPLAYS |
                FABRIC_CAPABILITY_SEGMENT_DISPLAYS;
    if (api_.GetAudioFormat && api_.FillAudioFrames)
      out.flags |= FABRIC_CAPABILITY_AUDIO;
    return ok();
  }
  FabricResult snapshot(FabricMachineSnapshot &out) noexcept override {
    PA2_OutputSnapshot source{};
    const uint32_t expected = api_.GetOutputSnapshotSize();
    if (expected != sizeof(source))
      return fail("GetOutputSnapshotSize",
                  "returned size=" + std::to_string(expected) +
                      "; expected size=" + std::to_string(sizeof(source)) +
                      "; DLL='" + path_ + "'");
    const uint32_t returned = api_.GetOutputSnapshot(&source, sizeof(source));
    if (returned != sizeof(source) || source.SizeBytes != sizeof(source) ||
        source.Version != PA2_OUTPUT_SNAPSHOT_VERSION)
      return fail("GetOutputSnapshot",
                  "returned size=" + std::to_string(returned) +
                      "; embedded size=" + std::to_string(source.SizeBytes) +
                      "; version=" + std::to_string(source.Version) +
                      "; DLL='" + path_ + "'");
    if (source.MatrixLampCount < 512 ||
        source.MatrixLampCount > PA2_MAX_MATRIX_LAMPS || source.ReelCount < 8 ||
        source.ReelCount > PA2_NUM_REELS ||
        source.AlphaSegmentedDisplayCount < 1 ||
        source.AlphaSegmentedDisplayCount > PA2_NUM_ALPHA_DISPLAYS ||
        source.LedCount < 256 || source.LedCount > PA2_MAX_LEDS)
      return fail("GetOutputSnapshot",
                  "invalid production counts: matrix lamps=" +
                      std::to_string(source.MatrixLampCount) + "; reels=" +
                      std::to_string(source.ReelCount) + "; alpha displays=" +
                      std::to_string(source.AlphaSegmentedDisplayCount) +
                      "; LEDs=" + std::to_string(source.LedCount));
    out.lamp_count = 512;
    out.reel_count = 8;
    out.character_display_count = 1;
    out.segment_display_count = 16;
    if (out.lamp_capacity < out.lamp_count ||
        out.reel_capacity < out.reel_count ||
        out.character_display_capacity < out.character_display_count ||
        out.segment_display_capacity < out.segment_display_count)
      return buffer_too_small("GetOutputSnapshot", out);
    if ((out.lamp_count && !out.lamps) || (out.reel_count && !out.reels) ||
        (out.character_display_count && !out.character_displays) ||
        (out.segment_display_count && !out.segment_displays))
      return invalid("snapshot output buffer is null");
    if (!std::isfinite(source.AlphaSegmented[0].Brightness))
      return fail("GetOutputSnapshot",
                  "alpha-display brightness is non-finite; index=0");
    for (uint32_t i = 0; i < 256; ++i)
      if (!std::isfinite(source.Leds[i].Brightness))
        return fail("GetOutputSnapshot",
                    "LED brightness is non-finite; index=" + std::to_string(i));
    for (uint32_t i = 0; i < out.lamp_count; ++i) {
      auto &d = out.lamps[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.lamp.%u", i);
      d.numerical_index = static_cast<int32_t>(i);
      d.logical_state = source.MatrixLamps[i].OnOff ? 1 : 0;
      if (!std::isfinite(source.MatrixLamps[i].Brightness))
        return fail("GetOutputSnapshot",
                    "matrix lamp brightness is non-finite; index=" +
                        std::to_string(i));
      d.brightness = source.MatrixLamps[i].Brightness;
    }
    for (uint32_t i = 0; i < out.reel_count; ++i) {
      auto &d = out.reels[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.reel.%u", i);
      d.numerical_index = static_cast<int32_t>(i);
      d.position = source.Reels[i].Position;
    }
    for (uint32_t i = 0; i < out.character_display_count; ++i) {
      auto &d = out.character_displays[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.alpha.%u", i);
      d.character_count = PA2_NUM_ALPHA_CHARS;
      d.character_capacity = FABRIC_CHARACTER_CAPACITY;
      for (uint32_t j = 0; j < PA2_NUM_ALPHA_CHARS; ++j) {
        d.characters[j] = source.AlphaSegmented[i].Segments[j];
        const uint8_t native = source.AlphaSegmented[i].DotComma[j];
        d.attributes[j] = native == static_cast<uint8_t>('.')   ? 1
                          : native == static_cast<uint8_t>(',') ? 2
                                                                : 0;
      }
    }
    for (uint32_t i = 0; i < out.segment_display_count; ++i) {
      auto &d = out.segment_displays[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_1;
      std::snprintf(d.identifier, sizeof(d.identifier),
                    "amber.seven-segment.%u", i);
      d.digit_count = 1;
      d.digit_capacity = FABRIC_SEGMENT_DIGIT_CAPACITY;
      uint64_t mask = 0;
      for (uint32_t segment = 0; segment < 8; ++segment)
        mask = (mask << 1) |
               (source.Leds[i * 16 + segment].OnOff ? UINT64_C(1) : 0);
      d.segment_masks[0] = mask;
    }
    if (snapshot_trace_count_ < 8) {
      uint32_t active = 0, changed_lamps = 0, changed_reels = 0,
               nonzero_displays = 0;
      for (uint32_t i = 0; i < 512; ++i) {
        const uint8_t now = source.MatrixLamps[i].OnOff ? 1 : 0;
        active += now;
        if (has_previous_snapshot_ && previous_lamps_[i] != now)
          ++changed_lamps;
        previous_lamps_[i] = now;
      }
      for (uint32_t i = 0; i < 8; ++i) {
        if (has_previous_snapshot_ &&
            previous_reels_[i] != source.Reels[i].Position)
          ++changed_reels;
        previous_reels_[i] = source.Reels[i].Position;
      }
      bool alpha_changed = false;
      for (uint32_t i = 0; i < 16; ++i) {
        if (has_previous_snapshot_ &&
            previous_alpha_[i] != source.AlphaSegmented[0].Segments[i])
          alpha_changed = true;
        previous_alpha_[i] = source.AlphaSegmented[0].Segments[i];
      }
      for (uint32_t i = 0; i < 16; ++i)
        if (out.segment_displays[i].segment_masks[0])
          ++nonzero_displays;
      amber_trace::Write(
          "GetOutputSnapshot: sequence=" + std::to_string(sequence_ + 1) +
          "; active_lamps=" + std::to_string(active) +
          "; changed_lamps=" + std::to_string(changed_lamps) +
          "; changed_reels=" + std::to_string(changed_reels) +
          "; alpha_changed=" + (alpha_changed ? "yes" : "no") +
          "; nonzero_displays=" + std::to_string(nonzero_displays) +
          "; Fabric result=0");
      has_previous_snapshot_ = true;
      ++snapshot_trace_count_;
    }
    if (snapshot_diagnostic_count_ < 16) {
      emit("AmberSnapshot", "sequence=" + std::to_string(sequence_ + 1) +
                                "; result=success");
      ++snapshot_diagnostic_count_;
    }
    out.sequence = ++sequence_;
    return ok();
  }
  FabricResult audio_format(FabricAudioFormat &out) noexcept override {
    if (!api_.GetAudioFormat || !api_.FillAudioFrames)
      return unsupported("audio format",
                         "required audio exports are unavailable");
    PA2_AudioFormat f{};
    f.SizeBytes = sizeof(f);
    f.Version = PA2_AUDIO_FORMAT_VERSION;
    const uint32_t returned = api_.GetAudioFormat(&f, sizeof(f));
    if (returned != sizeof(f) || f.SizeBytes != sizeof(f) ||
        f.Version != PA2_AUDIO_FORMAT_VERSION)
      return fail("GetAudioFormat", "returned size or structure identity is "
                                    "incompatible; returned size=" +
                                        std::to_string(returned) + "; DLL='" +
                                        path_ + "'");
    if (f.Format != PA2_AUDIO_FORMAT_PCM_S16 ||
        (f.Channels != 1 && f.Channels != 2) ||
        f.BitsPerSample != 16)
      return unsupported(
          "GetAudioFormat",
          "format is not representable interleaved PCM16; sample rate=" +
              std::to_string(f.SampleRate) +
              "; channels=" + std::to_string(f.Channels) +
              "; bits=" + std::to_string(f.BitsPerSample) +
              "; encoding=" + std::to_string(f.Format));
    out.sample_rate = f.SampleRate;
    out.channel_count = static_cast<uint16_t>(f.Channels);
    audio_sample_rate_ = f.SampleRate;
    audio_channel_count_ = f.Channels;
    out.bits_per_sample = 16;
    out.interleaved = out.signed_samples = out.little_endian = 1;
    amber_trace::Write(
        "GetAudioFormat: Amber return=" + std::to_string(returned) +
        "; sample rate=" + std::to_string(f.SampleRate) + "; channels=" +
        std::to_string(f.Channels) + "; bits=16; Fabric result=0");
    emit("AmberAudioFormat", "sample_rate=" + std::to_string(f.SampleRate) +
                                 "; channels=" + std::to_string(f.Channels) +
                                 "; bits_per_sample=16; result=success");
    return ok();
  }
  FabricResult read_audio(int16_t *samples, uint32_t capacity,
                          uint32_t &written) noexcept override {
    written = 0;
    if (!api_.FillAudioFrames || !api_.GetAudioFormat)
      return unsupported("FillAudioFrames",
                         "required audio exports are unavailable");
    if (capacity && !samples)
      return invalid("audio buffer is null");
    if (!audio_sample_rate_) {
      FabricAudioFormat ignored{};
      const FabricResult format_result = audio_format(ignored);
      if (format_result != FABRIC_OK)
        return format_result;
    }
    if (capacity > UINT32_MAX / audio_channel_count_ ||
        static_cast<uint64_t>(capacity) * audio_channel_count_ *
                sizeof(int16_t) >
            SIZE_MAX)
      return invalid("FillAudioFrames frame capacity overflows the interleaved "
                     "sample extent; requested frames=" +
                     std::to_string(capacity));
    const uint32_t permitted = static_cast<uint32_t>(
        std::min<uint64_t>(capacity, audio_frames_available_));
    written = permitted ? api_.FillAudioFrames(samples, permitted) : 0;
    if (written <= permitted)
      audio_frames_available_ -= written;
    total_audio_frames_ += written <= permitted ? written : 0;
    if (audio_trace_count_ < 16) {
      uint32_t nonzero = 0;
      if (samples && written <= permitted)
        for (uint64_t i = 0;
             i < static_cast<uint64_t>(written) * audio_channel_count_; ++i)
          if (samples[i])
            ++nonzero;
      amber_trace::Write(
          "FillAudioFrames: requested frames=" + std::to_string(capacity) +
          "; returned frames=" + std::to_string(written) +
          "; nonzero samples=" + std::to_string(nonzero) +
          "; total frames=" + std::to_string(total_audio_frames_) +
          (written <= permitted ? "; Fabric result=0" : "; Fabric result=7"));
      ++audio_trace_count_;
      emit("AmberReadAudio", "requested_frames=" +
                                 std::to_string(capacity) +
                                 "; permitted_frames=" +
                                 std::to_string(permitted) +
                                 "; returned_frames=" +
                                 std::to_string(written) +
                                 "; returned_samples=" +
                                 std::to_string(static_cast<uint64_t>(written) *
                                                audio_channel_count_) +
                                 "; remaining_queue=" +
                                 std::to_string(audio_frames_available_) +
                                 "; result=" +
                                 (written <= permitted ? "success" : "failure"));
      emit("AudioGenerate",
           "generated_frames=" + std::to_string(written) +
               "; generated_samples=" +
               std::to_string(static_cast<uint64_t>(written) *
                              audio_channel_count_) +
               "; queue_depth=" +
               std::to_string(audio_frames_available_) +
               "; result=" +
               (written <= permitted ? "success" : "failure"));
    }
    if (written > permitted) {
      const uint32_t invalid = written;
      written = 0;
      return fail("FillAudioFrames",
                  "returned frames=" + std::to_string(invalid) +
                      "; permitted frames=" + std::to_string(permitted));
    }
    return ok();
  }
  std::string last_error() const noexcept override { return error_; }

private:
  void emit(const std::string &operation, const std::string &metadata) noexcept {
    const std::string message = "[Fabric]\ncategory=amber.production\noperation=" +
                                operation + "\n" + metadata;
    if (diagnostic_) {
      try {
        diagnostic_(message.c_str(), diagnostic_user_data_);
      } catch (...) {
      }
    } else {
      amber_trace::Write("category=amber.production; operation=" + operation +
                         "; " + metadata);
    }
  }
  using Load = uint32_t (*)(uint8_t *, uint8_t *, uint8_t *, uint8_t *);
  bool load_roms(Load load, const std::vector<std::string> &paths,
                 const char *role) {
    if (paths.empty())
      return true;
    if (!load) {
      error_ = std::string("production Amber adapter: Load ") + role +
               " ROMs failed: required export is unavailable; DLL='" + path_ +
               "'";
      return false;
    }
    uint8_t *p[4]{};
    for (size_t i = 0; i < paths.size(); ++i)
      p[i] = reinterpret_cast<uint8_t *>(const_cast<char *>(paths[i].c_str()));
    const uint32_t native = load(p[0], p[1], p[2], p[3]);
    emit(std::strcmp(role, "program") == 0 ? "AmberLoadProgramRoms"
                                             : "AmberLoadSoundRoms",
         "slots=" + std::to_string(paths.size()) +
             "; native_return=" + std::to_string(native) +
             "; result=" + (native ? "success" : "failure"));
    for (size_t i = 0; i < paths.size(); ++i)
      amber_trace::Write(
          std::string("Load ") + role + " ROM: slot=" + std::to_string(i) +
          "; filename=" + std::filesystem::path(paths[i]).filename().string());
    amber_trace::Write(std::string("Load ") + role +
                       " ROMs: slots=" + std::to_string(paths.size()) +
                       "; Amber return=" + std::to_string(native) +
                       (native ? "; Fabric result=0" : "; Fabric result=3"));
    if (!native) {
      error_ = std::string("production Amber adapter: Load ") + role +
               " ROMs failed: Amber return=0; slots=" +
               std::to_string(paths.size()) + "; DLL='" + path_ + "'";
      return false;
    }
    return true;
  }
  FabricResult apply_configuration(const char *phase) {
    const std::string prefix = std::string(phase) + ": ";
    if (config_.flags & FABRIC_AMBER_CONFIGURE_REELS) {
      if (!api_.SetSteps || !api_.SetOptoInvert || !api_.SetOptoStart ||
          !api_.SetOptoEnd) {
        return unsupported(phase,
                           "missing reel export (SetSteps, SetOptoStart, "
                           "SetOptoEnd, or SetOptoInvert); DLL='" +
                               path_ + "'");
      }
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_REELS; ++i)
        if (config_.reels.apply_mask & (1u << i)) {
          const auto &r = config_.reels.reels[i];
          if (r.steps > 255 || r.opto_start > 255 || r.opto_end > 255 ||
              r.opto_invert > 255) {
            return unsupported(
                phase, "reel configuration value exceeds 8-bit ABI; index=" +
                           std::to_string(i));
          }
          if (!r.enabled)
            continue;
          const auto index = static_cast<uint8_t>(i);
          api_.SetSteps(index, static_cast<uint8_t>(r.steps));
          api_.SetOptoStart(index, static_cast<uint8_t>(r.opto_start));
          api_.SetOptoEnd(index, static_cast<uint8_t>(r.opto_end));
          api_.SetOptoInvert(index, static_cast<uint8_t>(r.opto_invert));
          emit("AmberConfigureReel",
               "index=" + std::to_string(i) +
                   "; enabled=" + std::to_string(r.enabled) +
                   "; steps=" + std::to_string(r.steps) +
                   "; opto_start=" + std::to_string(r.opto_start) +
                   "; opto_end=" + std::to_string(r.opto_end) +
                   "; opto_invert=" + std::to_string(r.opto_invert) +
                   "; result=success");
        }
      amber_trace::Write(prefix + "reels applied mask=" +
                         std::to_string(config_.reels.apply_mask));
    }
    if (config_.flags & FABRIC_AMBER_CONFIGURE_COINS) {
      if (!api_.SetCoinValue || !api_.SetCoinEnable) {
        return unsupported(
            phase, "missing export 'SetCoinValue' or 'SetCoinEnable'; DLL='" +
                       path_ + "'");
      }
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_COIN_CHANNELS; ++i)
        if (config_.coins.channel_apply_mask & (1u << i)) {
          const auto &c = config_.coins.channels[i];
          if (c.value > 255 || c.enabled > 255 || c.lockout_invert > 255) {
            return unsupported(phase,
                               "coin channel value exceeds 8-bit ABI; index=" +
                                   std::to_string(i));
          }
          const auto index = static_cast<uint8_t>(i);
          api_.SetCoinEnable(index, static_cast<uint8_t>(c.enabled));
          api_.SetCoinValue(index, static_cast<uint8_t>(c.value));
          if (!api_.SetLockoutInvert)
            return unsupported(phase,
                               "missing export 'SetLockoutInvert'; DLL='" +
                                   path_ + "'");
          api_.SetLockoutInvert(index, static_cast<uint8_t>(c.lockout_invert));
          emit("AmberConfigureCoin",
               "index=" + std::to_string(i) +
                   "; enabled=" + std::to_string(c.enabled) +
                   "; value=" + std::to_string(c.value) +
                   "; lockout_invert=" + std::to_string(c.lockout_invert) +
                   "; result=success");
        }
      if (config_.coins.configuration_flags &
          FABRIC_AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT) {
        if (!api_.SetLockoutVal || config_.coins.lockout_port_base > 255 ||
            config_.coins.lockout_port_value > 255)
          return unsupported(
              phase, "lockout port is unavailable or exceeds the 8-bit ABI");
        api_.SetLockoutVal(
            static_cast<uint8_t>(config_.coins.lockout_port_base),
            static_cast<uint8_t>(config_.coins.lockout_port_value));
      }
      if (config_.coins.route_apply_mask &&
          (!api_.SetEnable || !api_.SetCounterIn || !api_.SetCounterOut ||
           !api_.SetPortIndex || !api_.SetCoin || !api_.SetLevel ||
           !api_.SetFullLevel))
        return unsupported(phase, "one or more coin-route exports are missing");
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_COIN_ROUTES; ++i)
        if (config_.coins.route_apply_mask & (1u << i)) {
          const auto &r = config_.coins.routes[i];
          if (r.enabled > 255 || r.port_index > 255 || r.coin_code > 255 ||
              r.level > 255 || r.full_level > 255)
            return unsupported(phase,
                               "coin route value exceeds 8-bit ABI; index=" +
                                   std::to_string(i));
          const auto index = static_cast<uint8_t>(i);
          api_.SetEnable(index, static_cast<uint8_t>(r.enabled));
          api_.SetCounterIn(index, r.counter_in);
          api_.SetCounterOut(index, r.counter_out);
          api_.SetPortIndex(index, static_cast<uint8_t>(r.port_index));
          api_.SetCoin(index, static_cast<uint8_t>(r.coin_code));
          api_.SetLevel(index, static_cast<uint8_t>(r.level));
          api_.SetFullLevel(index, static_cast<uint8_t>(r.full_level));
        }
      amber_trace::Write(prefix + "coin channels applied mask=" +
                         std::to_string(config_.coins.channel_apply_mask) +
                         "; routes applied mask=" +
                         std::to_string(config_.coins.route_apply_mask));
    }
    if (config_.flags & FABRIC_AMBER_CONFIGURE_PERCENTAGE) {
      if (!api_.SetPercent) {
        return unsupported(phase,
                           "missing export 'SetPercent'; DLL='" + path_ + "'");
      }
      api_.SetPercent(static_cast<uint8_t>(config_.percentage_switch));
      emit("AmberConfigurePercentage",
           "raw_value=" + std::to_string(config_.percentage_switch) +
               "; result=success");
      amber_trace::Write(prefix + "percentage applied value=" +
                         std::to_string(config_.percentage_switch));
    }
    amber_trace::Write(prefix + "Fabric result=0");
    return FABRIC_OK;
  }
  void release_inputs() {
    if (started_ && !stopped_)
      for (uint8_t i : asserted_)
        api_.TurnSwitchOff(i);
    asserted_.clear();
  }
  FabricResult ok() {
    error_.clear();
    return FABRIC_OK;
  }
  FabricResult fail(const std::string &operation, const std::string &detail) {
    error_ = "production Amber adapter: " + operation + " failed: " + detail;
    amber_trace::Write(error_);
    return FABRIC_BACKEND_ERROR;
  }
  FabricResult invalid(const std::string &m) {
    error_ = "production Amber adapter: " + m;
    return FABRIC_INVALID_ARGUMENT;
  }
  FabricResult unsupported(const std::string &operation,
                           const std::string &detail) {
    error_ =
        "production Amber adapter: " + operation + " is unsupported: " + detail;
    return FABRIC_NOT_SUPPORTED;
  }
  FabricResult buffer_too_small(const char *operation,
                                const FabricMachineSnapshot &out) {
    error_ = "production Amber adapter: " + std::string(operation) +
             " failed: Fabric snapshot capacities are too small; lamps=" +
             std::to_string(out.lamp_capacity) +
             "/512; reels=" + std::to_string(out.reel_capacity) +
             "/8; alpha=" + std::to_string(out.character_display_capacity) +
             "/1; segment=" + std::to_string(out.segment_display_capacity) +
             "/16";
    return FABRIC_BUFFER_TOO_SMALL;
  }
  std::unique_ptr<AmberDynamicLibrary> library_;
  ProductionAmberApi api_{};
  std::vector<std::string> program_, sound_;
  FabricAmberConfigurationV1 config_{};
  std::set<uint8_t> asserted_;
  std::string error_, path_;
  uint64_t remainder_ = 0, sequence_ = 0, native_tick_ = 0;
  uint32_t advance_trace_count_ = 0, audio_trace_count_ = 0,
           reset_trace_count_ = 0, snapshot_trace_count_ = 0,
           snapshot_diagnostic_count_ = 0,
           audio_generate_diagnostic_count_ = 0;
  uint64_t total_audio_frames_ = 0;
  uint64_t audio_frames_available_ = 0, audio_frame_fraction_ = 0;
  uint32_t audio_sample_rate_ = 0, audio_channel_count_ = 0;
  std::array<uint8_t, 512> previous_lamps_{};
  std::array<int32_t, 8> previous_reels_{};
  std::array<uint16_t, 16> previous_alpha_{};
  bool started_ = false, stopped_ = false, has_previous_snapshot_ = false;
  FabricResult shutdown_result_ = FABRIC_OK;
  FabricDiagnosticCallback diagnostic_ = nullptr;
  void *diagnostic_user_data_ = nullptr;
};

template <typename T>
bool required(AmberDynamicLibrary &lib, T &fn, const char *name,
              const std::string &path, std::string &error) {
  fn = resolve<T>(lib, name);
  if (fn)
    return true;
  error = "Failed to initialise production Amber adapter for '" + path +
          "': required export '" + name +
          "' was not found during export resolution.";
  return false;
}
} // namespace

FabricResult
CreateProductionAmberInstance(const FabricLaunchRequest &request,
                          std::unique_ptr<AmberDynamicLibrary> library,
                          std::unique_ptr<FabricBackendInstance> &out,
                          std::string &error) noexcept {
  try {
    ProductionAmberApi a{};
#define REQ(n)                                                                 \
  if (!required(*library, a.n, #n, request.backend_path, error))               \
  return FABRIC_NOT_SUPPORTED
    REQ(Initialise);
    REQ(Shutdown);
    REQ(Reset);
    REQ(Run);
    REQ(LoadROM);
    REQ(GetOutputSnapshotSize);
    REQ(GetOutputSnapshot);
    REQ(TurnSwitchOn);
    REQ(TurnSwitchOff);
#undef REQ
#define OPT(n) a.n = resolve<decltype(a.n)>(*library, #n)
    OPT(LoadSoundROM);
    OPT(GetAudioFormat);
    OPT(FillAudioFrames);
    OPT(SetOptoInvert);
    OPT(SetOptoStart);
    OPT(SetOptoEnd);
    OPT(SetSteps);
    OPT(SetCoinValue);
    OPT(SetCoinEnable);
    OPT(SetLockoutVal);
    OPT(SetLockoutInvert);
    OPT(SetEnable);
    OPT(SetCounterIn);
    OPT(SetCounterOut);
    OPT(SetPortIndex);
    OPT(SetCoin);
    OPT(SetLevel);
    OPT(SetFullLevel);
    OPT(SetPercent);
#undef OPT
    std::vector<std::pair<uint32_t, std::string>> p, s;
    if (request.rom_resource_count) {
      for (uint32_t i = 0; i < request.rom_resource_count; ++i) {
        const auto &r = request.rom_resources[i];
        if (r.role == FABRIC_ROM_ROLE_PROGRAM)
          p.emplace_back(r.slot, r.path);
        else if (r.role == FABRIC_ROM_ROLE_SOUND)
          s.emplace_back(r.slot, r.path);
      }
    } else
      for (uint32_t i = 0; i < request.rom_path_count; ++i)
        p.emplace_back(i, request.rom_paths[i]);
    auto sort = [](auto &v) {
      std::sort(v.begin(), v.end(),
                [](const auto &a, const auto &b) { return a.first < b.first; });
    };
    sort(p);
    sort(s);
    std::vector<std::string> program, sound;
    for (auto &v : p)
      program.push_back(std::move(v.second));
    for (auto &v : s)
      sound.push_back(std::move(v.second));
    const auto *config = request.machine_configuration_size
                             ? static_cast<const FabricAmberConfigurationV1 *>(
                                   request.machine_configuration)
                             : nullptr;
    amber_trace::Write("selected for DLL='" +
                       std::string(request.backend_path) + "'");
    if (request.diagnostic_callback) {
      const char message[] = "[Fabric]\ncategory=amber.production\n"
                             "operation=AdapterSelected\nresult=success";
      try {
        request.diagnostic_callback(message, request.diagnostic_user_data);
      } catch (...) {
      }
    } else {
      amber_trace::Write("category=amber.production; operation=AdapterSelected; result=success");
    }
    out = std::make_unique<ProductionAmberInstance>(std::move(library), a,
                                           std::move(program), std::move(sound),
                                           config, request.backend_path,
                                           request.diagnostic_callback,
                                           request.diagnostic_user_data);
    return FABRIC_OK;
  } catch (const std::exception &e) {
    error = e.what();
    return FABRIC_INTERNAL_ERROR;
  }
}
} // namespace fabric
