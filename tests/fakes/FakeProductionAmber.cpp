#include "ProductionAmberAbi.h"

#include <algorithm>
#include <cstring>
#include <limits>

#ifndef FAKE_AMBER_SAMPLE_RATE
#define FAKE_AMBER_SAMPLE_RATE 44100
#endif
#ifndef FAKE_AMBER_CHANNELS
#define FAKE_AMBER_CHANNELS 2
#endif
#ifndef FAKE_AMBER_MAX_FRAMES_PER_READ
#define FAKE_AMBER_MAX_FRAMES_PER_READ UINT32_MAX
#endif

#ifdef _WIN32
#define PRODUCTION_EXPORT extern "C" __declspec(dllexport)
#else
#define PRODUCTION_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {
bool initialised;
uint64_t cycles;
uint32_t run_calls;
uint8_t switches[256];
uint8_t reel_steps[PA2_NUM_REELS];
uint8_t coin_values[16];
uint32_t coin_setter_calls[16][4];
uint8_t coin_setter_values[16][4];
uint8_t percentage;
uint32_t reset_count;
uint8_t configuration_stage;
uint32_t coin_in_calls, switch_on_calls, switch_off_calls;
uint8_t last_coin_channel, last_coin_value;
uint32_t mechanism_setter_calls[4];
uint32_t mechanism_values[4];
} // namespace

PRODUCTION_EXPORT float GetDLLVersion() { return 1.0f; }
#ifndef FAKE_AMBER_OMIT_INITIALISE
PRODUCTION_EXPORT uint8_t Initialise() {
  initialised = true;
  return 1;
}
#endif
PRODUCTION_EXPORT uint8_t Shutdown() {
  bool was = initialised;
  initialised = false;
  return was ? 1 : 0;
}
PRODUCTION_EXPORT void Reset() {
  cycles = 0;
  std::memset(reel_steps, 0, sizeof(reel_steps));
  std::memset(coin_values, 0, sizeof(coin_values));
  std::memset(coin_setter_calls, 0, sizeof(coin_setter_calls));
  std::memset(coin_setter_values, 0, sizeof(coin_setter_values));
  percentage = 0;
  configuration_stage = 0;
  coin_in_calls = switch_on_calls = switch_off_calls = 0;
  std::memset(mechanism_setter_calls, 0, sizeof(mechanism_setter_calls));
  std::memset(mechanism_values, 0, sizeof(mechanism_values));
  ++reset_count;
}
/* Musashi's return is observable production information, not the ABI's consumed
 * cycle count. Production cores may validly return zero. */
PRODUCTION_EXPORT int32_t Run(uint32_t requested) {
  cycles += requested;
  ++run_calls;
  return 0;
}
PRODUCTION_EXPORT uint32_t LoadROM(uint8_t *a, uint8_t *b, uint8_t *, uint8_t *) {
  return initialised && a && b ? 2u : 0u;
}
PRODUCTION_EXPORT uint32_t LoadSoundROM(uint8_t *a, uint8_t *, uint8_t *,
                                    uint8_t *) {
  return initialised && a ? 1u : 0u;
}
PRODUCTION_EXPORT void TurnSwitchOn(uint8_t i) {
  switches[i] = 1;
  ++switch_on_calls;
}
PRODUCTION_EXPORT void TurnSwitchOff(uint8_t i) {
  switches[i] = 0;
  ++switch_off_calls;
}
#ifndef FAKE_AMBER_OMIT_COIN_IN
PRODUCTION_EXPORT uint8_t CoinIn(uint8_t channel, uint8_t value) {
  ++coin_in_calls;
  last_coin_channel = channel;
  last_coin_value = value;
  return value == 4 ? 0 : 1;
}
#endif
#ifndef FAKE_AMBER_OMIT_SET_COMM_STYLE
PRODUCTION_EXPORT void SetCommStyle(uint8_t value) {
  ++mechanism_setter_calls[0];
  mechanism_values[0] = value;
}
#endif
PRODUCTION_EXPORT void SetCommInvert(uint8_t value) {
  ++mechanism_setter_calls[1];
  mechanism_values[1] = value;
}
PRODUCTION_EXPORT void SetCycles(uint32_t value) {
  ++mechanism_setter_calls[2];
  mechanism_values[2] = value;
}
PRODUCTION_EXPORT void SetEDCEnable(uint8_t value) {
  ++mechanism_setter_calls[3];
  mechanism_values[3] = value;
}
PRODUCTION_EXPORT void SetOptoInvert(uint8_t, uint8_t) {}
PRODUCTION_EXPORT void SetOptoStart(uint8_t, uint8_t) {}
PRODUCTION_EXPORT void SetOptoEnd(uint8_t, uint8_t) {}
#ifndef FAKE_AMBER_OMIT_SET_STEPS
PRODUCTION_EXPORT void SetSteps(uint8_t i, uint8_t value) {
  if (i < PA2_NUM_REELS)
    reel_steps[i] = value;
  configuration_stage = 1;
}
#endif
PRODUCTION_EXPORT void SetCoinValue(uint8_t i, uint8_t value) {
  if (i < 16) {
    coin_values[i] = value;
    ++coin_setter_calls[i][0];
    coin_setter_values[i][0] = value;
  }
  if (configuration_stage == 1)
    configuration_stage = 2;
}
PRODUCTION_EXPORT void SetCoinEnable(uint8_t i, uint8_t value) {
  if (i < 16) {
    ++coin_setter_calls[i][1];
    coin_setter_values[i][1] = value;
  }
}
PRODUCTION_EXPORT void SetLockoutVal(uint8_t i, uint8_t value) {
  if (i < 16) {
    ++coin_setter_calls[i][2];
    coin_setter_values[i][2] = value;
  }
}
PRODUCTION_EXPORT void SetLockoutInvert(uint8_t i, uint8_t value) {
  if (i < 16) {
    ++coin_setter_calls[i][3];
    coin_setter_values[i][3] = value;
  }
}
PRODUCTION_EXPORT void SetPercent(uint8_t value) {
  percentage = value;
  if (configuration_stage == 2)
    configuration_stage = 3;
}
PRODUCTION_EXPORT uint32_t GetOutputSnapshotSize() {
  return sizeof(PA2_OutputSnapshot);
}
PRODUCTION_EXPORT uint32_t GetOutputSnapshot(void *buffer, uint32_t size) {
  if (!initialised || !buffer || size < sizeof(PA2_OutputSnapshot))
    return 0;
  if (switches[250])
    return 0;
  auto &s = *static_cast<PA2_OutputSnapshot *>(buffer);
  std::memset(&s, 0, sizeof(s));
  s.SizeBytes = sizeof(s);
  s.Version = PA2_OUTPUT_SNAPSHOT_VERSION;
  s.MatrixLampCount = PA2_MAX_MATRIX_LAMPS;
  s.MatrixLamps[0].OnOff = switches[7];
  s.MatrixLamps[0].Brightness = switches[7] ? 0.75f : 0.0f;
  s.MatrixLamps[1].OnOff = percentage;
  s.MatrixLamps[1].Brightness = static_cast<float>(coin_values[0]);
  s.MatrixLamps[2].Brightness = static_cast<float>(run_calls);
  s.MatrixLamps[3].Brightness = static_cast<float>(configuration_stage);
  s.MatrixLamps[4].Brightness = static_cast<float>(reset_count);
  for (uint32_t channel = 0; channel < 6; ++channel)
    for (uint32_t setter = 0; setter < 4; ++setter) {
      const uint32_t lamp = 16 + channel * 4 + setter;
      s.MatrixLamps[lamp].OnOff = coin_setter_calls[channel][setter];
      s.MatrixLamps[lamp].Brightness =
          static_cast<float>(coin_setter_values[channel][setter]);
    }
  s.MatrixLamps[40].OnOff = coin_in_calls;
  s.MatrixLamps[40].Brightness = static_cast<float>(last_coin_channel);
  s.MatrixLamps[41].Brightness = static_cast<float>(last_coin_value);
  s.MatrixLamps[48].Brightness = static_cast<float>(coin_in_calls);
  s.MatrixLamps[42].Brightness = static_cast<float>(switch_on_calls);
  s.MatrixLamps[43].Brightness = static_cast<float>(switch_off_calls);
  for (uint32_t setter = 0; setter < 4; ++setter) {
    s.MatrixLamps[44 + setter].OnOff = mechanism_setter_calls[setter];
    s.MatrixLamps[44 + setter].Brightness =
        static_cast<float>(mechanism_values[setter]);
  }
  s.ReelCount = PA2_NUM_REELS;
  s.Reels[0].Position = static_cast<int32_t>(cycles + reel_steps[0]);
  s.AlphaSegmentedDisplayCount = 1;
  s.AlphaSegmented[0].SegmentCount = 16;
  s.AlphaSegmented[0].Segments[0] = 0x1234;
  s.AlphaSegmented[0].DotComma[0] = '.';
  float alpha_brightness = 0.5f;
  if (switches[240])
    alpha_brightness = 0.0f;
  else if (switches[241])
    alpha_brightness = 1.0f;
  else if (switches[248])
    alpha_brightness = 1.5f;
  else if (switches[249])
    alpha_brightness = std::numeric_limits<float>::quiet_NaN();
  s.AlphaSegmented[0].Brightness = alpha_brightness;
  s.LedCount = PA2_MAX_LEDS;
  for (uint32_t bit = 0; bit < 8; ++bit)
    s.Leds[bit].OnOff = (0x5au >> bit) & 1u;
  return sizeof(s);
}
PRODUCTION_EXPORT uint32_t GetAudioFormat(PA2_AudioFormat *f, uint32_t size) {
  if (!f || size < sizeof(*f))
    return 0;
  f->SizeBytes = sizeof(*f);
  f->Version = PA2_AUDIO_FORMAT_VERSION;
  f->SampleRate = FAKE_AMBER_SAMPLE_RATE;
  f->Channels = FAKE_AMBER_CHANNELS;
  f->BitsPerSample = 16;
  f->Format = PA2_AUDIO_FORMAT_PCM_S16;
  return sizeof(*f);
}
PRODUCTION_EXPORT uint32_t FillAudioFrames(int16_t *samples, uint32_t frames) {
  const uint32_t written = std::min(
      frames, static_cast<uint32_t>(FAKE_AMBER_MAX_FRAMES_PER_READ));
  for (uint32_t i = 0; i < written * FAKE_AMBER_CHANNELS; ++i)
    samples[i] = static_cast<int16_t>(100 + i);
  return written;
}
