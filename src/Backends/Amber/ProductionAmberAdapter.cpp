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
static_assert(96u <= FABRIC_DOT_MATRIX_MAX_WIDTH);
static_assert(8u <= FABRIC_DOT_MATRIX_MAX_HEIGHT);
static_assert(96u * 8u <= FABRIC_DOT_MATRIX_MAX_DOTS);
static_assert(alignof(PA2_OutputSnapshot) == 4,
              "production Amber snapshot must retain pack(4)");
static_assert(sizeof(PA2_OutputSnapshot) == 24812,
              "production Amber snapshot ABI size changed");
static_assert(sizeof(PA2_AudioFormat) == 24,
              "production Amber audio ABI size changed");
static_assert(sizeof(FabricAmberScorpion4ReelConfig) == 4);
static_assert(sizeof(FabricAmberScorpion4CoinConfig) == 4);
static_assert(sizeof(FabricAmberScorpion4HopperConfig) == 32);
static_assert(sizeof(FabricAmberScorpion4Config) == 152);
static_assert(offsetof(FabricAmberScorpion4Config, magic) == 0);
static_assert(offsetof(FabricAmberScorpion4Config, struct_size) == 4);
static_assert(offsetof(FabricAmberScorpion4Config, version) == 8);
static_assert(offsetof(FabricAmberScorpion4Config, reel_count) == 12);
static_assert(offsetof(FabricAmberScorpion4Config, reels) == 16);
static_assert(offsetof(FabricAmberScorpion4Config, dips) == 40);
static_assert(offsetof(FabricAmberScorpion4Config, stake) == 56);
static_assert(offsetof(FabricAmberScorpion4Config, prize) == 57);
static_assert(offsetof(FabricAmberScorpion4Config, percentage) == 58);
static_assert(offsetof(FabricAmberScorpion4Config, edc_enabled) == 59);
static_assert(offsetof(FabricAmberScorpion4Config, hopper_type) == 60);
static_assert(offsetof(FabricAmberScorpion4Config, hopper_count) == 61);
static_assert(offsetof(FabricAmberScorpion4Config, coin_channel_count) == 62);
static_assert(offsetof(FabricAmberScorpion4Config, reserved0) == 63);
static_assert(offsetof(FabricAmberScorpion4Config, coins) == 64);
static_assert(offsetof(FabricAmberScorpion4Config, hoppers) == 88);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, enabled) == 0);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, coin) == 1);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, lo_enable) == 2);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, hi_enable) == 3);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, coins_in) == 4);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, coins_out) == 8);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, level) == 12);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, full_level) == 16);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, lo_level) == 20);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, hi_level) == 24);
static_assert(offsetof(FabricAmberScorpion4HopperConfig, coins_refilled) == 28);
template <typename T>
T resolve(AmberDynamicLibrary &library, const char *name) {
  void *symbol = library.symbol(name);
  T result = nullptr;
  static_assert(sizeof(result) == sizeof(symbol), "function pointer size");
  std::memcpy(&result, &symbol, sizeof(result));
  return result;
}



enum class AmberMachine { System6, Mpu3, Mpu5, Epoch, M1, Scorpion4 };

class ProductionAmberInstance final : public FabricBackendInstance {
public:
  ProductionAmberInstance(std::unique_ptr<AmberDynamicLibrary> library, ProductionAmberApi api,
                 std::vector<std::string> program,
                 std::vector<std::string> sound,
                 const FabricAmberSystem6ConfigurationV2 *system6_configuration,
                 const FabricAmberMpu5ConfigurationV1 *mpu5_configuration,
                 const FabricAmberEpochConfigurationV1 *epoch_configuration,
                 const FabricAmberMpu3Config *mpu3_configuration,
                 const FabricAmberM1Config *m1_configuration,
                 const FabricAmberScorpion4Config *scorpion4_configuration,
                 std::string path, AmberMachine machine, FabricDiagnosticCallback diagnostic,
                 void *diagnostic_user_data)
      : library_(std::move(library)), api_(api), program_(std::move(program)),
        sound_(std::move(sound)), path_(std::move(path)), machine_(machine),
        diagnostic_(diagnostic), diagnostic_user_data_(diagnostic_user_data) {
    if (system6_configuration)
      system6_config_ = *system6_configuration;
    if (mpu5_configuration)
      mpu5_config_ = *mpu5_configuration;
    if (epoch_configuration)
      epoch_config_ = *epoch_configuration;
    if (mpu3_configuration)
      mpu3_config_ = *mpu3_configuration;
    if (m1_configuration)
      m1_config_ = *m1_configuration;
    if (scorpion4_configuration)
      scorpion4_config_ = *scorpion4_configuration;
  }
  ~ProductionAmberInstance() override {
    if (started_ && !stopped_)
      machine_ == AmberMachine::Mpu3 ? api_.Mpu3Shutdown() : (void)api_.Shutdown();
  }

  FabricResult initialise() noexcept override {
    emit("AmberInitialiseBegin", "result=pending");
    const uint8_t native = machine_ == AmberMachine::Mpu3 ? api_.Mpu3Initialise() : api_.Initialise();
    emit("AmberInitialiseEnd", "native_return=" + std::to_string(native) +
                                   "; result=" +
                                   (native ? "success" : "failure"));
    amber_trace::Write("Initialise: Amber return=" + std::to_string(native));
    if (!native)
      return fail("Initialise", "Amber return=0; DLL='" + path_ + "'");
    started_ = true;
    if (machine_ == AmberMachine::Epoch)
      api_.SetFlashROMMode(static_cast<uint8_t>(epoch_config_.flash_rom_mode));
    if (!load_roms(api_.LoadROM, program_, "program"))
      return machine_ == AmberMachine::Mpu3 ? FABRIC_BACKEND_ERROR
                                             : FABRIC_NOT_FOUND;
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
    if (machine_ == AmberMachine::Mpu3) {
      if (!api_.Mpu3Reset(0, 0, 0)) return fail("Reset", "Amber MPU3 return=0; DLL='" + path_ + "'");
    } else if (machine_ == AmberMachine::Mpu5) {
      const FabricResult configured = apply_mpu5_configuration("Pre-reset configuration");
      if (configured != FABRIC_OK) return configured;
      if (!api_.ResetMpu5()) return fail("Reset", "Amber return=0; machine='barcrest-mpu5'; DLL='" + path_ + "'");
    } else if (machine_ == AmberMachine::Epoch) {
      if (!api_.ResetEpoch()) return fail("Reset", "Amber return=0; machine='maygay-epoch'; DLL='" + path_ + "'");
    } else if (machine_ == AmberMachine::M1) {
      if (!api_.ResetM1()) return fail("Reset", "Amber return=0; machine='maygay-m1'; DLL='" + path_ + "'");
    } else if (machine_ == AmberMachine::Scorpion4) {
      if (!api_.ResetScorpion4()) return fail("Reset", "Amber return=0; machine='bellfruit-scorpion4'; DLL='" + path_ + "'");
    } else api_.Reset();
    asserted_.clear();
    asserted_coins_.clear();
    audio_frames_available_ = 0;
    audio_frame_fraction_ = 0;
    emit("AmberResetEnd", "result=success");
    if (reset_trace_count_ < 8)
      amber_trace::Write("Reset: native reset completed");
    if (machine_ != AmberMachine::Mpu5) {
      const FabricResult configured = machine_ == AmberMachine::Mpu3
          ? apply_mpu3_configuration() : machine_ == AmberMachine::Epoch
          ? apply_epoch_configuration("Post-reset configuration")
          : machine_ == AmberMachine::M1 ? apply_m1_configuration()
          : machine_ == AmberMachine::Scorpion4 ? apply_scorpion4_configuration()
          : apply_configuration("Reset configuration");
      if (configured != FABRIC_OK)
        return configured;
    }
    if (reset_trace_count_ < 8) {
      amber_trace::Write("Reset: Fabric result=0");
      ++reset_trace_count_;
    }
    return ok();
  }
  FabricResult advance(uint64_t ns) noexcept override {
    constexpr uint64_t tick_ns = UINT64_C(1000000);
    const uint32_t request = machine_ == AmberMachine::Mpu3 ? 675u :
        (machine_ == AmberMachine::M1 ? 2000u :
        (machine_ == AmberMachine::System6 ? 8000u :
        (machine_ == AmberMachine::Scorpion4 ? 16670u : 16000u)));

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
      const int32_t native = machine_ == AmberMachine::Mpu3
          ? api_.Mpu3Run(static_cast<INT32>(request)) : api_.Run(request);
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
    if (started_ && machine_ == AmberMachine::Mpu3) {
      api_.Mpu3Shutdown();
    } else if (started_ && !api_.Shutdown())
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
    if (input.kind == FABRIC_INPUT_COIN) {
      if (machine_ == AmberMachine::Mpu3) {
        (input.active ? api_.TurnSwitchOn : api_.TurnSwitchOff)(input.coin_channel);
        return ok();
      }
      if (input.coin_channel > 5)
        return invalid("Amber coin channel must be in the range 0..5");
      if (input.coin_value > (machine_ == AmberMachine::Scorpion4 ? 5u : 12u))
        return invalid(machine_ == AmberMachine::Scorpion4
                           ? "Scorpion 4 coin denomination must be in the range 0..5"
                           : "Amber coin denomination must be in the range 0..12");
      const uint16_t key = static_cast<uint16_t>(input.coin_channel << 8) |
                           input.coin_value;
      if (!input.active) {
        asserted_coins_.erase(key);
        return ok();
      }
      if (asserted_coins_.count(key))
        return ok();
      asserted_coins_.insert(key);
      const bool accepted = machine_ == AmberMachine::System6
          ? api_.CoinIn(input.coin_channel, input.coin_value) != 0
          : machine_ == AmberMachine::M1
          ? api_.CoinInM1(0, input.coin_channel, input.coin_value) != 0
          : machine_ == AmberMachine::Scorpion4
          ? api_.CoinInScorpion4(0, input.coin_channel, input.coin_value) != 0
          : api_.CoinInMpu5(0, input.coin_channel, input.coin_value) != 0;
      if (coin_input_diagnostic_count_ < 64) {
        emit("AmberCoinInput", "channel=" + std::to_string(input.coin_channel) +
                                   "; value=" + std::to_string(input.coin_value) +
                                   "; result=" +
                                   (accepted ? "accepted" : "rejected"));
        ++coin_input_diagnostic_count_;
      }
      if (!accepted) {
        error_ = "production Amber adapter: coin was rejected";
        return FABRIC_INPUT_REJECTED;
      }
      return ok();
    }
    if (input.numerical_index < 0 || input.numerical_index > 255)
      return invalid(
          "production Amber switch index must be in the range 0..255");
    const uint8_t index = static_cast<uint8_t>(input.numerical_index);
    if (machine_ == AmberMachine::Mpu3) {
      (input.active ? api_.TurnSwitchOn : api_.TurnSwitchOff)(index);
      if (input.active) asserted_.insert(index); else asserted_.erase(index);
    } else if (input.active) {
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
    out.flags |= FABRIC_CAPABILITY_COIN_INPUT;
    if (machine_ == AmberMachine::Scorpion4)
      out.flags |= FABRIC_CAPABILITY_DOT_MATRIX_DISPLAYS;
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
    bool invalid_counts = false;
    switch (machine_) {
    case AmberMachine::Mpu3:
      invalid_counts = source.ReelCount != 4 || source.AlphaSegmentedDisplayCount != 1 ||
          source.AlphaDotDisplayCount != 0 || source.LedDisplayCount > PA2_NUM_LED_DISPLAYS;
      break;
    case AmberMachine::Mpu5:
      invalid_counts = source.MatrixLampCount != 320 || source.ReelCount != 8 ||
          source.AlphaSegmentedDisplayCount > PA2_NUM_ALPHA_DISPLAYS || source.LedDisplayCount > 40;
      break;
    case AmberMachine::Epoch:
      invalid_counts = source.MatrixLampCount != 512 || source.LedCount != 512 ||
          source.ReelCount != 8 || source.AlphaSegmentedDisplayCount != 1 ||
          source.AlphaDotDisplayCount != 1 || source.LedDisplayCount != 40 ||
          source.ElectronicMechCount != 1 || source.MeterCount != 6 ||
          source.DipCount != 16 || source.HopperCount != 2;
      /* Epoch dot-alpha normalization remains outside the current contract. */
      break;
    case AmberMachine::M1:
      invalid_counts = source.MatrixLampCount != 256 || source.TriacLampCount != 8 ||
          source.ReelCount != 6 || source.AlphaSegmentedDisplayCount != 1 ||
          source.AlphaDotDisplayCount != 0 || source.LedDisplayCount != 0 ||
          source.ElectronicMechCount != 1 || source.MeterCount != 6 ||
          source.DipCount != 16 || source.HopperCount != 2 ||
          source.AlphaSegmented[0].SegmentCount != 16;
      break;
    case AmberMachine::Scorpion4:
      invalid_counts = source.MatrixLampCount != 256 || source.DirectLampCount ||
          source.FloLampCount || source.PrismLampCount || source.LedCount ||
          source.TriacLampCount || source.FluorescentLampCount || source.DiscoLampCount ||
          source.ReelCount != 6 || source.AlphaSegmentedDisplayCount != 2 ||
          source.AlphaDotDisplayCount != 1 || source.LedDisplayCount != 32 ||
          source.ElectronicMechCount || source.MechanicalMechCount ||
          source.CoinEntryLampCount || source.MeterCount != 6 || source.TubeCount ||
          source.DipCount != 16 || source.HopperCount != 2;
      break;
    case AmberMachine::System6:
      invalid_counts = source.MatrixLampCount < 512 || source.MatrixLampCount > 512 ||
          source.ReelCount < 8 || source.ReelCount > 8 ||
          source.AlphaSegmentedDisplayCount < 1 || source.AlphaSegmentedDisplayCount > 2 ||
          source.LedCount < 256 || source.LedCount > 512;
      break;
    }
    if (invalid_counts)
      return fail("GetOutputSnapshot",
                  "invalid production counts: matrix lamps=" +
                      std::to_string(source.MatrixLampCount) + "; reels=" +
                      std::to_string(source.ReelCount) + "; alpha displays=" +
                      std::to_string(source.AlphaSegmentedDisplayCount) +
                      "; LEDs=" + std::to_string(source.LedCount));
    if (machine_ == AmberMachine::Scorpion4)
      for (uint32_t i = 0; i < 2; ++i)
        if (source.AlphaSegmented[i].SegmentCount != 14 &&
            source.AlphaSegmented[i].SegmentCount != 16)
          return fail("GetOutputSnapshot", "invalid Scorpion 4 alpha segment count");
    out.lamp_count = source.MatrixLampCount;
    out.reel_count = source.ReelCount;
    out.character_display_count = (machine_ == AmberMachine::Mpu5 || machine_ == AmberMachine::Mpu3 || machine_ == AmberMachine::Scorpion4) ? source.AlphaSegmentedDisplayCount : 1;
    out.segment_display_count = machine_ == AmberMachine::System6 ? 16 : source.LedDisplayCount;
    out.dot_matrix_display_count = machine_ == AmberMachine::Scorpion4 ? 1 : 0;
    if (out.lamp_capacity < out.lamp_count ||
        out.reel_capacity < out.reel_count ||
        out.character_display_capacity < out.character_display_count ||
        out.segment_display_capacity < out.segment_display_count ||
        out.dot_matrix_display_capacity < out.dot_matrix_display_count)
      return buffer_too_small("GetOutputSnapshot", out);
    if ((out.lamp_count && !out.lamps) || (out.reel_count && !out.reels) ||
        (out.character_display_count && !out.character_displays) ||
        (out.segment_display_count && !out.segment_displays) ||
        (out.dot_matrix_display_count && !out.dot_matrix_displays))
      return invalid("snapshot output buffer is null");
    for (uint32_t i = 0; i < out.character_display_count; ++i) {
      const float brightness = source.AlphaSegmented[i].Brightness;
      if (!std::isfinite(brightness))
        return fail("GetOutputSnapshot",
                    "alpha-display brightness is non-finite; index=" +
                        std::to_string(i));
      if (brightness < 0.0f || brightness > 1.0f)
        return fail("GetOutputSnapshot",
                    "alpha-display brightness is outside 0.0..1.0; index=" +
                        std::to_string(i) + "; value=" +
                        std::to_string(brightness));
    }
    if (out.dot_matrix_display_count) {
      const float brightness = source.AlphaDot[0].Brightness;
      if (!std::isfinite(brightness) || brightness < 0.0f || brightness > 1.0f)
        return fail("GetOutputSnapshot",
                    "dot-matrix brightness is outside finite 0.0..1.0 range");
    }
    for (uint32_t i = 0; i < (machine_ == AmberMachine::System6 ? 256u : source.LedDisplayCount); ++i)
      if (!std::isfinite(machine_ == AmberMachine::System6 ? source.Leds[i].Brightness : source.LedDisplays[i].Brightness))
        return fail("GetOutputSnapshot",
                    "LED brightness is non-finite; index=" + std::to_string(i));
    for (uint32_t i = 0; i < out.lamp_count; ++i) {
      auto &d = out.lamps[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_CURRENT;
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
      d.struct_version = FABRIC_ABI_VERSION_CURRENT;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.reel.%u", i);
      d.numerical_index = static_cast<int32_t>(i);
      d.position = source.Reels[i].Position;
    }
    for (uint32_t i = 0; i < out.character_display_count; ++i) {
      auto &d = out.character_displays[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_CURRENT;
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
      d.brightness = source.AlphaSegmented[i].Brightness;
    }
    for (uint32_t i = 0; i < out.segment_display_count; ++i) {
      auto &d = out.segment_displays[i];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_CURRENT;
      std::snprintf(d.identifier, sizeof(d.identifier),
                    "amber.seven-segment.%u", i);
      d.digit_count = 1;
      d.digit_capacity = FABRIC_SEGMENT_DIGIT_CAPACITY;
      if (machine_ != AmberMachine::System6) {
        d.segment_masks[0] = source.LedDisplays[i].OnOff;
      } else {
        uint64_t mask = 0;
        for (uint32_t segment = 0; segment < 8; ++segment)
          mask = (mask << 1) |
                 (source.Leds[i * 16 + segment].OnOff ? UINT64_C(1) : 0);
        d.segment_masks[0] = mask;
      }
    }
    if (out.dot_matrix_display_count) {
      auto &d = out.dot_matrix_displays[0];
      d = {};
      d.struct_size = sizeof(d);
      d.struct_version = FABRIC_ABI_VERSION_CURRENT;
      std::snprintf(d.identifier, sizeof(d.identifier), "amber.dot-matrix.0");
      d.width = 96;
      d.height = 8;
      d.dot_count = d.width * d.height;
      d.dot_capacity = FABRIC_DOT_MATRIX_MAX_DOTS;
      for (uint32_t cell = 0; cell < PA2_NUM_ALPHA_CHARS; ++cell)
        for (uint32_t column = 0; column < 5; ++column)
          for (uint32_t y = 0; y < 8; ++y)
            d.dots[y * d.width + cell * 6 + column] =
                (source.AlphaDot[0].Columns[cell][column] >> y) & 1u;
      d.brightness = source.AlphaDot[0].Brightness;
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
      for (uint32_t i = 0; i < out.segment_display_count; ++i)
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
        f.BitsPerSample != 16 || ((machine_ == AmberMachine::M1 || machine_ == AmberMachine::Scorpion4) &&
        (f.SampleRate != 48000 || f.Channels != 2)))
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
  FabricResult apply_scorpion4_configuration() {
    const auto &c = scorpion4_config_;
    for (uint8_t i = 0; i < FABRIC_AMBER_SCORPION4_REEL_COUNT; ++i) {
      const auto &r = c.reels[i];
      api_.SetSteps(i, r.steps); api_.SetOptoStart(i, r.opto_start);
      api_.SetOptoEnd(i, r.opto_end); api_.SetOptoInvert(i, r.opto_invert);
    }
    for (uint8_t i = 0; i < FABRIC_AMBER_SCORPION4_DIP_COUNT; ++i) api_.SetDIP(i, c.dips[i]);
    api_.SetStake(c.stake); api_.SetPrize(c.prize); api_.SetPercent(c.percentage);
    api_.SetEDCEnable(c.edc_enabled);
    for (uint8_t i = 0; i < FABRIC_AMBER_SCORPION4_COIN_CHANNEL_COUNT; ++i) {
      api_.SetCoinEnable(i, c.coins[i].enabled); api_.SetCoinValue(i, c.coins[i].value);
    }
    api_.SetHopperType(c.hopper_type);
    for (uint8_t i = 0; i < FABRIC_AMBER_SCORPION4_HOPPER_COUNT; ++i) {
      const auto &h = c.hoppers[i];
      api_.SetHopperEnable(i, h.enabled); api_.SetHopperCoinsIn(i, h.coins_in);
      api_.SetHopperCoinsOut(i, h.coins_out); api_.SetHopperCoin(i, h.coin);
      api_.SetHopperLevel(i, h.level); api_.SetHopperFullLevel(i, h.full_level);
      api_.SetHopperLoEnable(i, h.lo_enable); api_.SetHopperLoLevel(i, h.lo_level);
      api_.SetHopperHiEnable(i, h.hi_enable); api_.SetHopperHiLevel(i, h.hi_level);
      api_.SetHopperCoinsRefilled(i, h.coins_refilled);
    }
    return FABRIC_OK;
  }
  FabricResult apply_m1_configuration() {
    for (uint8_t i = 0; i < FABRIC_AMBER_M1_REEL_COUNT; ++i) {
      const auto &r = m1_config_.reels[i];
      api_.SetSteps(i, r.steps); api_.SetOptoStart(i, r.opto_start);
      api_.SetOptoEnd(i, r.opto_end); api_.SetOptoInvert(i, r.opto_invert);
    }
    for (uint8_t i = 0; i < FABRIC_AMBER_M1_DIP_COUNT; ++i)
      api_.SetDIP(i, m1_config_.dips[i]);
    api_.SetPercent(m1_config_.percentage_key);
    api_.SetEDCEnable(m1_config_.edc_enabled);
    for (uint8_t i = 0; i < FABRIC_AMBER_M1_HOPPER_COUNT; ++i) {
      const auto &h = m1_config_.hoppers[i];
      api_.SetHopperEnable(i,h.enabled); api_.SetHopperCoinsIn(i,h.coins_in);
      api_.SetHopperCoinsOut(i,h.coins_out); api_.SetHopperOptoEnable(i,h.opto_enable);
      api_.SetHopperOptoReturn(i,h.opto_return); api_.SetHopperMotorEnable(i,h.motor_enable);
      api_.SetHopperCoin(i,h.coin); api_.SetHopperLevel(i,h.level);
      api_.SetHopperFullLevel(i,h.full_level); api_.SetHopperLoEnable(i,h.lo_enable);
      api_.SetHopperLoInvert(i,h.lo_invert); api_.SetHopperLoSwitch(i,h.lo_switch);
      api_.SetHopperLoLevel(i,h.lo_level); api_.SetHopperHiEnable(i,h.hi_enable);
      api_.SetHopperHiInvert(i,h.hi_invert); api_.SetHopperHiSwitch(i,h.hi_switch);
      api_.SetHopperHiLevel(i,h.hi_level); api_.SetHopperLoIndicator(i,h.lo_indicator);
      api_.SetHopperHiIndicator(i,h.hi_indicator); api_.SetHopperCoinsRefilled(i,h.coins_refilled);
    }
    return FABRIC_OK;
  }
  FabricResult apply_mpu3_configuration() {
    for (uint8_t i = 0; i < FABRIC_AMBER_MPU3_REEL_COUNT; ++i) {
      const auto &r = mpu3_config_.reels[i];
      api_.SetSteps(i, r.steps);
      api_.SetOptoStart(i, r.opto_start);
      api_.SetOptoEnd(i, r.opto_end);
      api_.SetOptoInvert(i, r.opto_invert);
    }
    for (uint8_t i = 0; i < FABRIC_AMBER_MPU3_DIP_COUNT; ++i)
      if (!api_.Mpu3SetDIP(i, mpu3_config_.dips[i] != 0))
        return fail("MPU3 DIP configuration", "native setter returned 0");
    return FABRIC_OK;
  }
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
      if (machine_ == AmberMachine::Mpu3) {
        std::ostringstream detail;
        detail << "production Amber adapter: Load " << role
               << " ROMs failed: machine=barcrest-mpu3; rom_count="
               << paths.size() << "; native_return=" << native;
        for (size_t i = 0; i < paths.size(); ++i)
          detail << "; slot" << i << "="
                 << std::filesystem::path(paths[i]).filename().string();
        detail << "; DLL='" << path_ << "'";
        error_ = detail.str();
      } else {
        error_ = std::string("production Amber adapter: Load ") + role +
                 " ROMs failed: Amber return=0; slots=" +
                 std::to_string(paths.size()) + "; DLL='" + path_ + "'";
      }
      return false;
    }
    return true;
  }
  FabricResult apply_mpu5_configuration(const char *phase) {
    auto missing = [&](const char *name) {
      return unsupported(phase, "requested MPU5 configuration export '" +
          std::string(name) + "' is missing; machine='barcrest-mpu5'; DLL='" +
          path_ + "'; stage=configuration");
    };
    const auto &o = mpu5_config_.options;
    const uint32_t options =
        (mpu5_config_.flags & FABRIC_AMBER_MPU5_CONFIGURE_OPTIONS)
            ? o.apply_mask : 0;
#define MPU_OPTION(bit, member, export_name, cast_type)                         \
    if (options & (bit)) {                                                      \
      if (!api_.export_name) return missing(#export_name);                     \
      api_.export_name(static_cast<cast_type>(o.member));                      \
    }
    MPU_OPTION(FABRIC_AMBER_MPU5_OPTION_CHARACTERISER_ADDRESS,
               characteriser_address, SetCharacteriserAddress, uint32_t);
    MPU_OPTION(FABRIC_AMBER_MPU5_OPTION_PIC_MODE, pic_mode, SetPICMode, uint8_t);
    MPU_OPTION(FABRIC_AMBER_MPU5_OPTION_SEC_FITTED, sec_fitted, SetSECFitted, uint8_t);
    MPU_OPTION(FABRIC_AMBER_MPU5_OPTION_HOPPER_TYPE, hopper_type, SetHopperType, uint8_t);
#undef MPU_OPTION
    if (options & (FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_0 |
                   FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_1)) {
      if (!api_.SetReelJumperProfile) return missing("SetReelJumperProfile");
      if (options & FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_0)
        api_.SetReelJumperProfile(0, static_cast<uint8_t>(o.reel_jumper_profile_0));
      if (options & FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_1)
        api_.SetReelJumperProfile(1, static_cast<uint8_t>(o.reel_jumper_profile_1));
    }
    if (mpu5_config_.flags & FABRIC_AMBER_MPU5_CONFIGURE_REELS) {
      if (!api_.SetSteps) return missing("SetSteps");
      if (!api_.SetOptoStart) return missing("SetOptoStart");
      if (!api_.SetOptoEnd) return missing("SetOptoEnd");
      if (!api_.SetOptoInvert) return missing("SetOptoInvert");
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_REELS; ++i)
        if (mpu5_config_.reels.apply_mask & (UINT32_C(1) << i)) {
          const auto &r = mpu5_config_.reels.reels[i];
          const auto index = static_cast<uint8_t>(i);
          api_.SetSteps(index, static_cast<uint8_t>(r.steps));
          api_.SetOptoStart(index, static_cast<uint8_t>(r.opto_start));
          api_.SetOptoEnd(index, static_cast<uint8_t>(r.opto_end));
          api_.SetOptoInvert(index, static_cast<uint8_t>(r.opto_invert));
        }
    }
    if (mpu5_config_.flags & FABRIC_AMBER_MPU5_CONFIGURE_COINS) {
      if (!api_.SetCommStyle) return missing("SetCommStyle");
      if (!api_.SetCommInvert) return missing("SetCommInvert");
      if (!api_.SetCycles) return missing("SetCycles");
      if (!api_.SetEDCEnable) return missing("SetEDCEnable");
      if (!api_.SetCoinEnable) return missing("SetCoinEnable");
      if (!api_.SetCoinValue) return missing("SetCoinValue");
      if (!api_.SetLockoutInvert) return missing("SetLockoutInvert");
      const auto &coins = mpu5_config_.coins;
      api_.SetCommStyle(static_cast<uint8_t>(coins.communication_style));
      api_.SetCommInvert(static_cast<uint8_t>(coins.communication_invert));
      api_.SetCycles(coins.pulse_cycles);
      api_.SetEDCEnable(static_cast<uint8_t>(coins.edc_enabled));
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_COIN_CHANNELS; ++i)
        if (coins.apply_mask & (UINT32_C(1) << i)) {
          const auto &c = coins.channels[i];
          const auto index = static_cast<uint8_t>(i);
          api_.SetCoinEnable(index, static_cast<uint8_t>(c.enabled));
          api_.SetCoinValue(index, static_cast<uint8_t>(c.value));
          api_.SetLockoutInvert(index, static_cast<uint8_t>(c.lockout_invert));
        }
    }
    if (options & FABRIC_AMBER_MPU5_OPTION_DIPS) {
      if (!api_.SetDIP) return missing("SetDIP");
      for (uint8_t i = 0; i < FABRIC_AMBER_MPU5_DIP_COUNT; ++i)
        api_.SetDIP(i, static_cast<uint8_t>((o.dip_switch_bits >> i) & 1u));
    }
    if (options & FABRIC_AMBER_MPU5_OPTION_STAKE) {
      if (!api_.SetStake) return missing("SetStake");
      api_.SetStake(static_cast<uint8_t>(o.stake));
    }
    if (options & FABRIC_AMBER_MPU5_OPTION_PRIZE) {
      if (!api_.SetPrize) return missing("SetPrize");
      api_.SetPrize(static_cast<uint8_t>(o.prize));
    }
    if (options & FABRIC_AMBER_MPU5_OPTION_PERCENTAGE) {
      if (!api_.SetPercent) return missing("SetPercent");
      api_.SetPercent(static_cast<uint8_t>(o.percentage));
    }
    amber_trace::Write(std::string(phase) +
                       ": MPU5 configuration applied before reset; Fabric result=0");
    return FABRIC_OK;
  }
  FabricResult apply_epoch_configuration(const char *phase) {
    auto missing = [&](const char *name) { return unsupported(phase,
        "requested Epoch configuration export '" + std::string(name) +
        "' is missing; machine='maygay-epoch'; DLL='" + path_ + "'"); };
    const auto &c = epoch_config_;
    if (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_REELS) {
      if (!api_.SetSteps || !api_.SetOptoStart || !api_.SetOptoEnd || !api_.SetOptoInvert)
        return missing("reel setters");
      for (uint32_t i = 0; i < 8; ++i) if (c.reel_apply_mask & (UINT32_C(1) << i)) {
        const auto &r = c.reels[i];
        api_.SetSteps(static_cast<uint8_t>(i), static_cast<uint8_t>(r.steps));
        api_.SetOptoStart(static_cast<uint8_t>(i), static_cast<uint8_t>(r.opto_start));
        api_.SetOptoEnd(static_cast<uint8_t>(i), static_cast<uint8_t>(r.opto_end));
        api_.SetOptoInvert(static_cast<uint8_t>(i), static_cast<uint8_t>(r.opto_invert));
      }
    }
    if (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_REEL_EXT) {
      if (!api_.SetReelExt) return missing("SetReelExt");
      api_.SetReelExt(static_cast<uint8_t>(c.reel_ext));
    }
    if (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_COINS) {
      if (!api_.SetCommStyle || !api_.SetCommInvert || !api_.SetCycles || !api_.SetEDCEnable ||
          !api_.SetCoinEnable || !api_.SetCoinValue || !api_.SetLockoutVal || !api_.SetLockoutInvert)
        return missing("coin setters");
      api_.SetCommStyle(static_cast<uint8_t>(c.communication_style));
      api_.SetCommInvert(static_cast<uint8_t>(c.communication_invert));
      api_.SetCycles(c.pulse_cycles); api_.SetEDCEnable(static_cast<uint8_t>(c.edc_enabled));
      for (uint32_t i = 0; i < 6; ++i) if (c.coin_apply_mask & (UINT32_C(1) << i)) {
        const auto &v = c.coins[i]; const auto n = static_cast<uint8_t>(i);
        api_.SetCoinEnable(n, static_cast<uint8_t>(v.enabled));
        api_.SetCoinValue(n, static_cast<uint8_t>(v.value));
        api_.SetLockoutVal(n, static_cast<uint8_t>(v.lockout_value));
        api_.SetLockoutInvert(n, static_cast<uint8_t>(v.lockout_invert));
      }
    }
    const uint32_t o = (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_OPTIONS) ? c.options_apply_mask : 0;
    if (o & FABRIC_AMBER_EPOCH_OPTION_DIPS) { if (!api_.SetDIP) return missing("SetDIP");
      for (uint8_t i = 0; i < 16; ++i) api_.SetDIP(i, static_cast<uint8_t>((c.dip_switch_bits >> i) & 1)); }
    if (o & FABRIC_AMBER_EPOCH_OPTION_STAKE) { if (!api_.SetStake) return missing("SetStake"); api_.SetStake(static_cast<uint8_t>(c.stake)); }
    if (o & FABRIC_AMBER_EPOCH_OPTION_PRIZE) { if (!api_.SetPrize) return missing("SetPrize"); api_.SetPrize(static_cast<uint8_t>(c.prize)); }
    if (o & FABRIC_AMBER_EPOCH_OPTION_PERCENTAGE) { if (!api_.SetPercent) return missing("SetPercent"); api_.SetPercent(static_cast<uint8_t>(c.percentage)); }
    amber_trace::Write(std::string(phase) + ": Epoch configuration applied after reset; Fabric result=0");
    return FABRIC_OK;
  }
  FabricResult apply_configuration(const char *phase) {
    if (machine_ == AmberMachine::Mpu5)
      return apply_mpu5_configuration(phase);
    const std::string prefix = std::string(phase) + ": ";
    if (system6_config_.flags & FABRIC_AMBER_SYSTEM6_CONFIGURE_REELS) {
      if (!api_.SetSteps || !api_.SetOptoInvert || !api_.SetOptoStart ||
          !api_.SetOptoEnd) {
        return unsupported(phase,
                           "missing reel export (SetSteps, SetOptoStart, "
                           "SetOptoEnd, or SetOptoInvert); DLL='" +
                               path_ + "'");
      }
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_REELS; ++i)
        if (system6_config_.reels.apply_mask & (1u << i)) {
          const auto &r = system6_config_.reels.reels[i];
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
                         std::to_string(system6_config_.reels.apply_mask));
    }
    if (system6_config_.flags & FABRIC_AMBER_SYSTEM6_CONFIGURE_COINS) {
      if (!api_.SetCommStyle || !api_.SetCommInvert || !api_.SetCycles ||
          !api_.SetEDCEnable || !api_.SetCoinEnable || !api_.SetCoinValue)
        return unsupported(phase, "one or more coin configuration exports are missing; DLL='" + path_ + "'");
      api_.SetCommStyle(static_cast<uint8_t>(system6_config_.coins.coin_communication_style));
      api_.SetCommInvert(static_cast<uint8_t>(system6_config_.coins.coin_communication_invert));
      api_.SetCycles(system6_config_.coins.coin_pulse_cycles);
      api_.SetEDCEnable(static_cast<uint8_t>(system6_config_.coins.coin_edc_enabled));
      if (coin_configuration_diagnostic_count_ < 64) {
        emit("AmberCoinMechanismConfigured",
             "style=" + std::to_string(system6_config_.coins.coin_communication_style) +
                 "; invert=" + std::to_string(system6_config_.coins.coin_communication_invert) +
                 "; cycles=" + std::to_string(system6_config_.coins.coin_pulse_cycles) +
                 "; edc=" + std::to_string(system6_config_.coins.coin_edc_enabled));
        ++coin_configuration_diagnostic_count_;
      }
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_COIN_CHANNELS; ++i)
        if (system6_config_.coins.channel_apply_mask & (1u << i)) {
          const auto &c = system6_config_.coins.channels[i];
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
          if (coin_configuration_diagnostic_count_ < 64) {
            emit("AmberCoinChannelConfigured",
                 "index=" + std::to_string(i) +
                     "; enabled=" + std::to_string(c.enabled) +
                     "; value=" + std::to_string(c.value) +
                     "; lockoutInvert=" + std::to_string(c.lockout_invert) +
                     "; result=success");
            ++coin_configuration_diagnostic_count_;
          }
        }
      if (system6_config_.coins.route_apply_mask &&
          (!api_.SetEnable || !api_.SetCounterIn || !api_.SetCounterOut ||
           !api_.SetPortIndex || !api_.SetCoin || !api_.SetLevel ||
           !api_.SetFullLevel))
        return unsupported(phase, "one or more coin-route exports are missing");
      for (uint32_t i = 0; i < FABRIC_AMBER_MAX_COIN_ROUTES; ++i)
        if (system6_config_.coins.route_apply_mask & (1u << i)) {
          const auto &r = system6_config_.coins.routes[i];
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
                         std::to_string(system6_config_.coins.channel_apply_mask) +
                         "; routes applied mask=" +
                         std::to_string(system6_config_.coins.route_apply_mask));
    }
    if (system6_config_.flags & FABRIC_AMBER_SYSTEM6_CONFIGURE_PERCENTAGE) {
      if (!api_.SetPercent) {
        return unsupported(phase,
                           "missing export 'SetPercent'; DLL='" + path_ + "'");
      }
      api_.SetPercent(static_cast<uint8_t>(system6_config_.percentage_switch));
      emit("AmberConfigurePercentage",
           "raw_value=" + std::to_string(system6_config_.percentage_switch) +
               "; result=success");
      amber_trace::Write(prefix + "percentage applied value=" +
                         std::to_string(system6_config_.percentage_switch));
    }
    amber_trace::Write(prefix + "Fabric result=0");
    return FABRIC_OK;
  }
  void release_inputs() {
    if (started_ && !stopped_ && machine_ == AmberMachine::Mpu3)
      for (uint8_t i : asserted_) api_.TurnSwitchOff(i);
    else if (started_ && !stopped_)
      for (uint8_t i : asserted_)
        api_.TurnSwitchOff(i);
    asserted_.clear();
    asserted_coins_.clear();
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
             "/16; dot-matrix=" +
             std::to_string(out.dot_matrix_display_capacity) + "/1";
    return FABRIC_BUFFER_TOO_SMALL;
  }
  std::unique_ptr<AmberDynamicLibrary> library_;
  ProductionAmberApi api_{};
  std::vector<std::string> program_, sound_;
  FabricAmberSystem6ConfigurationV2 system6_config_{};
  FabricAmberMpu5ConfigurationV1 mpu5_config_{};
  FabricAmberEpochConfigurationV1 epoch_config_{};
  FabricAmberMpu3Config mpu3_config_{};
  FabricAmberM1Config m1_config_{};
  FabricAmberScorpion4Config scorpion4_config_{};
  std::set<uint8_t> asserted_;
  std::set<uint16_t> asserted_coins_;
  std::string error_, path_;
  uint64_t remainder_ = 0, sequence_ = 0, native_tick_ = 0;
  uint32_t advance_trace_count_ = 0, audio_trace_count_ = 0,
           reset_trace_count_ = 0, snapshot_trace_count_ = 0,
           snapshot_diagnostic_count_ = 0,
           audio_generate_diagnostic_count_ = 0;
  uint32_t coin_input_diagnostic_count_ = 0,
           coin_configuration_diagnostic_count_ = 0;
  uint64_t total_audio_frames_ = 0;
  uint64_t audio_frames_available_ = 0, audio_frame_fraction_ = 0;
  uint32_t audio_sample_rate_ = 0, audio_channel_count_ = 0;
  std::array<uint8_t, 512> previous_lamps_{};
  std::array<int32_t, 8> previous_reels_{};
  std::array<uint16_t, 16> previous_alpha_{};
  bool started_ = false, stopped_ = false, has_previous_snapshot_ = false;
  AmberMachine machine_ = AmberMachine::System6;
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
    const AmberMachine machine = std::strcmp(request.machine_identifier, "barcrest-mpu3") == 0
        ? AmberMachine::Mpu3 : (std::strcmp(request.machine_identifier, "barcrest-mpu5") == 0
        ? AmberMachine::Mpu5 : (std::strcmp(request.machine_identifier, "maygay-epoch") == 0
        ? AmberMachine::Epoch : (std::strcmp(request.machine_identifier, "maygay-m1") == 0
        ? AmberMachine::M1 : (std::strcmp(request.machine_identifier, "bellfruit-scorpion4") == 0
        ? AmberMachine::Scorpion4 : AmberMachine::System6))));
    const bool mpu5 = machine == AmberMachine::Mpu5;
    const bool epoch = machine == AmberMachine::Epoch;
    const bool mpu3 = machine == AmberMachine::Mpu3;
    const bool m1 = machine == AmberMachine::M1;
    const bool scorpion4 = machine == AmberMachine::Scorpion4;
#define REQ(n)                                                                 \
  if (!required(*library, a.n, #n, request.backend_path, error))               \
  return FABRIC_NOT_SUPPORTED
    if (!mpu3) {
      REQ(Initialise); REQ(Shutdown); REQ(Run); REQ(LoadROM);
      REQ(GetOutputSnapshotSize); REQ(GetOutputSnapshot);
      REQ(TurnSwitchOn); REQ(TurnSwitchOff);
    }
#undef REQ
    if (mpu3) {
#define MPU3REQ(member, name) if (!required(*library, a.member, name, request.backend_path, error)) return FABRIC_NOT_SUPPORTED
      MPU3REQ(Mpu3Initialise, "Initialise"); MPU3REQ(Mpu3Shutdown, "Shutdown");
      MPU3REQ(Mpu3Reset, "Reset"); MPU3REQ(Mpu3Run, "Run"); MPU3REQ(LoadROM, "LoadROM");
      MPU3REQ(Mpu3SetDIP, "SetDIP");
      MPU3REQ(GetOutputSnapshotSize, "GetOutputSnapshotSize");
      MPU3REQ(GetOutputSnapshot, "GetOutputSnapshot");
      MPU3REQ(TurnSwitchOn, "TurnSwitchOn"); MPU3REQ(TurnSwitchOff, "TurnSwitchOff");
      MPU3REQ(SetSteps, "SetSteps"); MPU3REQ(SetOptoStart, "SetOptoStart");
      MPU3REQ(SetOptoEnd, "SetOptoEnd"); MPU3REQ(SetOptoInvert, "SetOptoInvert");
#undef MPU3REQ
    } else if (m1) {
#define M1REQ(member, name) if (!required(*library, a.member, name, request.backend_path, error)) return FABRIC_NOT_SUPPORTED
      M1REQ(ResetM1, "Reset"); M1REQ(CoinInM1, "CoinIn");
      M1REQ(SetSteps, "SetSteps"); M1REQ(SetOptoStart, "SetOptoStart");
      M1REQ(SetOptoEnd, "SetOptoEnd"); M1REQ(SetOptoInvert, "SetOptoInvert");
      M1REQ(SetDIP, "SetDIP"); M1REQ(SetPercent, "SetPercent"); M1REQ(SetEDCEnable, "SetEDCEnable");
#define H(name) M1REQ(name, #name)
      H(SetHopperEnable); H(SetHopperCoinsIn); H(SetHopperCoinsOut); H(SetHopperOptoEnable);
      H(SetHopperOptoReturn); H(SetHopperMotorEnable); H(SetHopperCoin); H(SetHopperLevel);
      H(SetHopperFullLevel); H(SetHopperLoEnable); H(SetHopperLoInvert); H(SetHopperLoSwitch);
      H(SetHopperLoLevel); H(SetHopperHiEnable); H(SetHopperHiInvert); H(SetHopperHiSwitch);
      H(SetHopperHiLevel); H(SetHopperLoIndicator); H(SetHopperHiIndicator); H(SetHopperCoinsRefilled);
#undef H
#undef M1REQ
    } else if (scorpion4) {
#define SC4REQ(member, name) if (!required(*library, a.member, name, request.backend_path, error)) return FABRIC_NOT_SUPPORTED
      SC4REQ(ResetScorpion4, "Reset"); SC4REQ(CoinInScorpion4, "CoinIn");
      SC4REQ(LoadSoundROM, "LoadSoundROM"); SC4REQ(GetAudioFormat, "GetAudioFormat");
      SC4REQ(FillAudioFrames, "FillAudioFrames");
      SC4REQ(SetSteps, "SetSteps"); SC4REQ(SetOptoStart, "SetOptoStart");
      SC4REQ(SetOptoEnd, "SetOptoEnd"); SC4REQ(SetOptoInvert, "SetOptoInvert");
      SC4REQ(SetDIP, "SetDIP"); SC4REQ(SetStake, "SetStake");
      SC4REQ(SetPrize, "SetPrize"); SC4REQ(SetPercent, "SetPercent");
      SC4REQ(SetEDCEnable, "SetEDCEnable"); SC4REQ(SetCoinValue, "SetCoinValue");
      SC4REQ(SetCoinEnable, "SetCoinEnable"); SC4REQ(SetHopperType, "SetHopperType");
#define H(name) SC4REQ(name, #name)
      H(SetHopperEnable); H(SetHopperCoinsIn); H(SetHopperCoinsOut); H(SetHopperCoin);
      H(SetHopperLevel); H(SetHopperFullLevel); H(SetHopperLoEnable); H(SetHopperLoLevel);
      H(SetHopperHiEnable); H(SetHopperHiLevel); H(SetHopperCoinsRefilled);
#undef H
#undef SC4REQ
    } else if (mpu5 || epoch) {
      if (!required(*library, mpu5 ? a.ResetMpu5 : a.ResetEpoch, "Reset", request.backend_path, error) ||
          !required(*library, a.CoinInMpu5, "CoinIn", request.backend_path, error))
        return FABRIC_NOT_SUPPORTED;
      if (epoch && !required(*library, a.SetFlashROMMode, "SetFlashROMMode", request.backend_path, error))
        return FABRIC_NOT_SUPPORTED;
    } else {
#define SYSREQ(n) if (!required(*library, a.n, #n, request.backend_path, error)) return FABRIC_NOT_SUPPORTED
      SYSREQ(Reset); SYSREQ(CoinIn); SYSREQ(SetCommStyle); SYSREQ(SetCommInvert);
      SYSREQ(SetCycles); SYSREQ(SetEDCEnable); SYSREQ(SetCoinValue);
      SYSREQ(SetCoinEnable); SYSREQ(SetLockoutInvert);
#undef SYSREQ
    }
#define OPT(n) a.n = resolve<decltype(a.n)>(*library, #n)
    OPT(LoadSoundROM); OPT(GetAudioFormat); OPT(FillAudioFrames);
    if (m1 && (!a.LoadSoundROM || !a.GetAudioFormat || !a.FillAudioFrames)) {
      /* M1 exposes sound loading and streaming as one coherent optional
       * capability. A soundless M1 can run without it; a partial ABI is never
       * advertised or called. */
      a.LoadSoundROM = nullptr;
      a.GetAudioFormat = nullptr;
      a.FillAudioFrames = nullptr;
    }
    if (machine == AmberMachine::System6) {
      OPT(SetOptoInvert); OPT(SetOptoStart); OPT(SetOptoEnd); OPT(SetSteps);
      OPT(SetCommStyle); OPT(SetCommInvert); OPT(SetCycles); OPT(SetEDCEnable);
      OPT(SetCoinValue); OPT(SetCoinEnable); OPT(SetLockoutInvert);
    } else if (mpu5 && request.machine_configuration_size) {
      const auto &configuration =
          *static_cast<const FabricAmberMpu5ConfigurationV1 *>(
              request.machine_configuration);
      if (configuration.flags & FABRIC_AMBER_MPU5_CONFIGURE_REELS) {
        OPT(SetOptoInvert); OPT(SetOptoStart); OPT(SetOptoEnd); OPT(SetSteps);
      }
      if (configuration.flags & FABRIC_AMBER_MPU5_CONFIGURE_COINS) {
        OPT(SetCommStyle); OPT(SetCommInvert); OPT(SetCycles); OPT(SetEDCEnable);
        OPT(SetCoinValue); OPT(SetCoinEnable); OPT(SetLockoutInvert);
      }
      if (configuration.flags & FABRIC_AMBER_MPU5_CONFIGURE_OPTIONS) {
        const uint32_t mask = configuration.options.apply_mask;
        if (mask & FABRIC_AMBER_MPU5_OPTION_DIPS) OPT(SetDIP);
        if (mask & FABRIC_AMBER_MPU5_OPTION_STAKE) OPT(SetStake);
        if (mask & FABRIC_AMBER_MPU5_OPTION_PRIZE) OPT(SetPrize);
        if (mask & FABRIC_AMBER_MPU5_OPTION_PERCENTAGE) OPT(SetPercent);
        if (mask & FABRIC_AMBER_MPU5_OPTION_CHARACTERISER_ADDRESS)
          OPT(SetCharacteriserAddress);
        if (mask & FABRIC_AMBER_MPU5_OPTION_PIC_MODE) OPT(SetPICMode);
        if (mask & FABRIC_AMBER_MPU5_OPTION_SEC_FITTED) OPT(SetSECFitted);
        if (mask & FABRIC_AMBER_MPU5_OPTION_HOPPER_TYPE) OPT(SetHopperType);
        if (mask & (FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_0 |
                    FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_1))
          OPT(SetReelJumperProfile);
      }
    }
    if (epoch && request.machine_configuration_size) {
      const auto &c = *static_cast<const FabricAmberEpochConfigurationV1 *>(request.machine_configuration);
      if (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_REELS) { OPT(SetOptoInvert); OPT(SetOptoStart); OPT(SetOptoEnd); OPT(SetSteps); }
      if (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_REEL_EXT) OPT(SetReelExt);
      if (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_COINS) { OPT(SetCommStyle); OPT(SetCommInvert); OPT(SetCycles); OPT(SetEDCEnable); OPT(SetCoinValue); OPT(SetCoinEnable); OPT(SetLockoutVal); OPT(SetLockoutInvert); }
      if (c.flags & FABRIC_AMBER_EPOCH_CONFIGURE_OPTIONS) { OPT(SetDIP); OPT(SetStake); OPT(SetPrize); OPT(SetPercent); }
    }
    if (machine == AmberMachine::System6) {
      OPT(SetEnable); OPT(SetCounterIn); OPT(SetCounterOut); OPT(SetPortIndex);
      OPT(SetCoin); OPT(SetLevel); OPT(SetFullLevel);
      OPT(SetPercent);
    }
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
    const auto *system6_config = machine == AmberMachine::System6 && request.machine_configuration_size
        ? static_cast<const FabricAmberSystem6ConfigurationV2 *>(
              request.machine_configuration) : nullptr;
    const auto *mpu5_config = mpu5 && request.machine_configuration_size
        ? static_cast<const FabricAmberMpu5ConfigurationV1 *>(
              request.machine_configuration) : nullptr;
    const auto *epoch_config = epoch && request.machine_configuration_size
        ? static_cast<const FabricAmberEpochConfigurationV1 *>(request.machine_configuration) : nullptr;
    const auto *mpu3_config = mpu3 && request.machine_configuration_size
        ? static_cast<const FabricAmberMpu3Config *>(request.machine_configuration) : nullptr;
    const auto *m1_config = m1 && request.machine_configuration_size
        ? static_cast<const FabricAmberM1Config *>(request.machine_configuration) : nullptr;
    const auto *scorpion4_config = scorpion4
        ? static_cast<const FabricAmberScorpion4Config *>(request.machine_configuration) : nullptr;
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
                                           system6_config, mpu5_config, epoch_config,
                                           mpu3_config, m1_config, scorpion4_config,
                                           request.backend_path,
                                           machine,
                                           request.diagnostic_callback,
                                           request.diagnostic_user_data);
    return FABRIC_OK;
  } catch (const std::exception &e) {
    error = e.what();
    return FABRIC_INTERNAL_ERROR;
  }
}
} // namespace fabric
