#include "ProductionAmberAbi.h"

#include <algorithm>
#include <cstring>
#include <limits>

#ifndef FAKE_AMBER_SAMPLE_RATE
#define FAKE_AMBER_SAMPLE_RATE 48000
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
uint8_t reel_opto_start[PA2_NUM_REELS], reel_opto_end[PA2_NUM_REELS];
uint8_t reel_opto_invert[PA2_NUM_REELS];
uint8_t coin_values[16];
uint8_t coin_enabled[16], coin_lockout_invert[16];
uint8_t percentage;
uint32_t reset_count;
uint8_t configuration_stage;
uint32_t switch_on_calls, switch_off_calls, coin_in_calls, lockout_val_calls;
uint8_t last_coin_channel, last_coin_value;
uint8_t comm_style, comm_invert, edc_enabled;
uint32_t pulse_cycles, mechanism_calls;
uint8_t dips[16], stake, prize, pic_mode, sec_fitted, hopper_type;
uint8_t reel_jumper[2];
uint32_t characteriser_address, dip_calls, configuration_calls;
uint32_t reset_observed_configuration_calls;
} // namespace

PRODUCTION_EXPORT float GetDLLVersion() { return 1.0f; }
#ifndef FAKE_AMBER_OMIT_INITIALISE
PRODUCTION_EXPORT uint8_t Initialise() {
  cycles = run_calls = reset_count = configuration_calls = 0;
  std::memset(reel_steps, 0, sizeof(reel_steps));
  std::memset(reel_opto_start, 0, sizeof(reel_opto_start));
  std::memset(reel_opto_end, 0, sizeof(reel_opto_end));
  std::memset(reel_opto_invert, 0, sizeof(reel_opto_invert));
  std::memset(coin_values, 0, sizeof(coin_values));
  std::memset(coin_enabled, 0, sizeof(coin_enabled));
  std::memset(coin_lockout_invert, 0, sizeof(coin_lockout_invert));
  initialised = true;
  return 1;
}
#endif
PRODUCTION_EXPORT uint8_t Shutdown() {
  bool was = initialised;
  initialised = false;
  return was ? 1 : 0;
}
PRODUCTION_EXPORT uint8_t Reset() {
  cycles = 0;
  configuration_stage = 0;
  ++reset_count;
  reset_observed_configuration_calls = configuration_calls;
  return switches[252] ? 0 : 1;
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
PRODUCTION_EXPORT void TurnSwitchOn(uint8_t i) { switches[i] = 1; ++switch_on_calls; }
PRODUCTION_EXPORT void TurnSwitchOff(uint8_t i) { switches[i] = 0; ++switch_off_calls; }
#ifndef FAKE_AMBER_OMIT_COIN_IN
PRODUCTION_EXPORT uint8_t CoinIn(uint8_t mechanism, uint8_t channel, uint8_t value) {
  ++coin_in_calls; last_coin_channel = static_cast<uint8_t>(mechanism * 64 + channel); last_coin_value = value;
  return value == 11 ? 0 : 1;
}
#endif
#ifndef FAKE_AMBER_OMIT_SET_COMM_STYLE
PRODUCTION_EXPORT void SetCommStyle(uint8_t value) { comm_style = value; ++mechanism_calls; ++configuration_calls; }
#endif
PRODUCTION_EXPORT void SetCommInvert(uint8_t value) { comm_invert = value; ++mechanism_calls; ++configuration_calls; }
PRODUCTION_EXPORT void SetCycles(uint32_t value) { pulse_cycles = value; ++mechanism_calls; ++configuration_calls; }
PRODUCTION_EXPORT void SetEDCEnable(uint8_t value) { edc_enabled = value; ++mechanism_calls; ++configuration_calls; }
#ifndef FAKE_AMBER_OMIT_REEL_OPTO
PRODUCTION_EXPORT void SetOptoInvert(uint8_t i, uint8_t value) { reel_opto_invert[i] = value; }
PRODUCTION_EXPORT void SetOptoStart(uint8_t i, uint8_t value) { reel_opto_start[i] = value; }
PRODUCTION_EXPORT void SetOptoEnd(uint8_t i, uint8_t value) { reel_opto_end[i] = value; }
#endif
#ifndef FAKE_AMBER_OMIT_SET_STEPS
PRODUCTION_EXPORT void SetSteps(uint8_t i, uint8_t value) {
  if (i < PA2_NUM_REELS)
    reel_steps[i] = value;
  configuration_stage = 1;
}
#endif
PRODUCTION_EXPORT void SetCoinValue(uint8_t i, uint8_t value) {
  if (i < 16)
    coin_values[i] = value;
  if (configuration_stage == 1)
    configuration_stage = 2;
}
PRODUCTION_EXPORT void SetCoinEnable(uint8_t i, uint8_t value) { coin_enabled[i] = value; }
PRODUCTION_EXPORT void SetLockoutInvert(uint8_t i, uint8_t value) { coin_lockout_invert[i] = value; }
PRODUCTION_EXPORT void SetLockoutVal(uint8_t, uint8_t) { ++lockout_val_calls; }
PRODUCTION_EXPORT void SetPercent(uint8_t value) {
  percentage = value; ++configuration_calls;
  if (configuration_stage == 2)
    configuration_stage = 3;
}
#ifndef FAKE_AMBER_OMIT_SET_DIP
PRODUCTION_EXPORT void SetDIP(uint8_t i, uint8_t value) { dips[i] = value ? 1 : 0; ++dip_calls; ++configuration_calls; }
#endif
PRODUCTION_EXPORT void SetStake(uint8_t value) { stake = value; ++configuration_calls; }
PRODUCTION_EXPORT void SetPrize(uint8_t value) { prize = value; ++configuration_calls; }
PRODUCTION_EXPORT void SetCharacteriserAddress(uint32_t value) { characteriser_address = value; ++configuration_calls; }
#ifndef FAKE_AMBER_OMIT_SET_PIC_MODE
PRODUCTION_EXPORT void SetPICMode(uint8_t value) { pic_mode = value; ++configuration_calls; }
#endif
PRODUCTION_EXPORT void SetSECFitted(uint8_t value) { sec_fitted = value; ++configuration_calls; }
PRODUCTION_EXPORT void SetHopperType(uint8_t value) { hopper_type = value; ++configuration_calls; }
#ifndef FAKE_AMBER_OMIT_SET_REEL_JUMPER
PRODUCTION_EXPORT void SetReelJumperProfile(uint8_t i, uint8_t value) { reel_jumper[i] = value; ++configuration_calls; }
#endif
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
  s.MatrixLampCount = 320;
  s.MatrixLamps[0].OnOff = switches[7];
  s.MatrixLamps[0].Brightness = switches[7] ? 0.75f : 0.0f;
  s.MatrixLamps[1].OnOff = percentage;
  s.MatrixLamps[1].Brightness = static_cast<float>(coin_values[0]);
  s.MatrixLamps[2].Brightness = static_cast<float>(run_calls);
  s.MatrixLamps[3].Brightness = static_cast<float>(configuration_stage);
  s.MatrixLamps[4].Brightness = static_cast<float>(reset_count);
  s.MatrixLamps[5].Brightness = static_cast<float>(coin_in_calls);
  s.MatrixLamps[6].Brightness = static_cast<float>(last_coin_channel * 16 + last_coin_value);
  s.MatrixLamps[7].Brightness = static_cast<float>(switch_on_calls + switch_off_calls);
  s.MatrixLamps[8].Brightness = static_cast<float>(mechanism_calls);
  s.MatrixLamps[9].Brightness = static_cast<float>(pulse_cycles);
  s.MatrixLamps[10].Brightness = static_cast<float>(lockout_val_calls);
  s.MatrixLamps[11].Brightness = static_cast<float>(comm_style + comm_invert + edc_enabled);
  s.MatrixLamps[12].Brightness = static_cast<float>(reel_opto_start[1]);
  s.MatrixLamps[13].Brightness = static_cast<float>(reel_opto_end[1]);
  s.MatrixLamps[14].Brightness = static_cast<float>(reel_opto_invert[1]);
  s.MatrixLamps[15].Brightness = static_cast<float>(coin_enabled[2]);
  s.MatrixLamps[16].Brightness = static_cast<float>(coin_values[2]);
  s.MatrixLamps[17].Brightness = static_cast<float>(coin_lockout_invert[2]);
  s.MatrixLamps[18].Brightness = static_cast<float>(percentage);
  s.MatrixLamps[19].Brightness = static_cast<float>(stake);
  s.MatrixLamps[20].Brightness = static_cast<float>(prize);
  s.MatrixLamps[21].Brightness = static_cast<float>(pic_mode);
  s.MatrixLamps[22].Brightness = static_cast<float>(sec_fitted);
  s.MatrixLamps[23].Brightness = static_cast<float>(hopper_type);
  s.MatrixLamps[24].Brightness = static_cast<float>(reel_jumper[0] * 10 + reel_jumper[1]);
  s.MatrixLamps[25].Brightness = static_cast<float>(dip_calls);
  s.MatrixLamps[26].Brightness = static_cast<float>(configuration_calls);
  s.MatrixLamps[27].Brightness = static_cast<float>(reset_observed_configuration_calls);
  s.MatrixLamps[28].Brightness = static_cast<float>(characteriser_address);
  uint32_t dip_bits = 0;
  for (uint32_t i = 0; i < 16; ++i) dip_bits |= static_cast<uint32_t>(dips[i]) << i;
  s.MatrixLamps[29].Brightness = static_cast<float>(dip_bits);
  s.ReelCount = PA2_NUM_REELS;
  s.Reels[0].Position = static_cast<int32_t>(cycles + reel_steps[0]);
  s.AlphaSegmentedDisplayCount = 2;
  s.AlphaSegmented[1].SegmentCount = 14;
  s.AlphaSegmented[1].Segments[0] = 0x5678;
  s.AlphaSegmented[1].Brightness = 0.75f;
  s.LedDisplayCount = 40;
  for (uint32_t i = 0; i < 40; ++i) { s.LedDisplays[i].OnOff = i; s.LedDisplays[i].Brightness = 0.5f; }
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
  s.LedCount = 0;
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
