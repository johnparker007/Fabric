#include "ProductionAmberAbi.h"
#include <algorithm>
#include <cstring>

#ifdef _WIN32
#define PRODUCTION_EXPORT extern "C" __declspec(dllexport)
#else
#define PRODUCTION_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace {
bool initialised;
uint64_t cycles;
uint32_t reset_count, coin_calls;
uint8_t last_mech, last_channel, last_value;
uint8_t switches[256];
uint8_t percent;
}

PRODUCTION_EXPORT uint8_t Initialise() { initialised = true; return 1; }
PRODUCTION_EXPORT uint8_t Shutdown() { bool was = initialised; initialised = false; return was ? 1 : 0; }
#ifndef FAKE_MPU5_RESET_FAILS
PRODUCTION_EXPORT uint8_t Reset() { cycles = 0; percent = 0; ++reset_count; return 1; }
#else
PRODUCTION_EXPORT uint8_t Reset() { ++reset_count; return 0; }
#endif
PRODUCTION_EXPORT int32_t Run(uint32_t requested) { cycles += requested; return 0; }
PRODUCTION_EXPORT uint32_t LoadROM(uint8_t *a, uint8_t *, uint8_t *, uint8_t *) { return initialised && a ? 1u : 0u; }
PRODUCTION_EXPORT void TurnSwitchOn(uint8_t i) { switches[i] = 1; }
PRODUCTION_EXPORT void TurnSwitchOff(uint8_t i) { switches[i] = 0; }
#ifndef FAKE_MPU5_OMIT_COIN_IN
PRODUCTION_EXPORT uint8_t CoinIn(uint8_t mech, uint8_t channel, uint8_t value) { ++coin_calls; last_mech = mech; last_channel = channel; last_value = value; return value == 11 ? 0 : 1; }
#endif
PRODUCTION_EXPORT void SetPercent(uint8_t value) { percent = value; }
PRODUCTION_EXPORT uint32_t GetOutputSnapshotSize() { return sizeof(PA2_OutputSnapshot); }
PRODUCTION_EXPORT uint32_t GetOutputSnapshot(void *buffer, uint32_t size) {
  if (!initialised || !buffer || size < sizeof(PA2_OutputSnapshot)) return 0;
  auto &s = *static_cast<PA2_OutputSnapshot *>(buffer);
  std::memset(&s, 0, sizeof(s));
  s.SizeBytes = sizeof(s);
  s.Version = switches[251] ? 999 : PA2_OUTPUT_SNAPSHOT_VERSION;
  s.MatrixLampCount = switches[250] ? 319 : 320;
  s.ReelCount = 8;
  s.AlphaSegmentedDisplayCount = 2;
  s.LedDisplayCount = 40;
  s.LedCount = 0;
  s.MatrixLamps[0].Brightness = static_cast<float>(cycles);
  s.MatrixLamps[1].Brightness = static_cast<float>(reset_count);
  s.MatrixLamps[2].Brightness = static_cast<float>(coin_calls);
  s.MatrixLamps[3].Brightness = static_cast<float>(last_mech * 100 + last_channel * 10 + last_value);
  s.MatrixLamps[4].Brightness = static_cast<float>(percent);
  s.Reels[0].Position = static_cast<int32_t>(cycles);
  s.AlphaSegmented[0].Brightness = 0.25f;
  s.AlphaSegmented[0].Segments[0] = 0x1111;
  s.AlphaSegmented[0].DotComma[0] = '.';
  s.AlphaSegmented[1].Brightness = 0.75f;
  s.AlphaSegmented[1].Segments[0] = 0x2222;
  s.AlphaSegmented[1].DotComma[0] = ',';
  for (uint32_t i = 0; i < 40; ++i) { s.LedDisplays[i].OnOff = 0x80u | i; s.LedDisplays[i].Brightness = 1.0f; }
  s.StatusLED = 3; // multi-state, intentionally not flattened by Fabric.
  return sizeof(s);
}
PRODUCTION_EXPORT uint32_t GetAudioFormat(PA2_AudioFormat *f, uint32_t size) { if (!f || size < sizeof(*f)) return 0; f->SizeBytes=sizeof(*f); f->Version=PA2_AUDIO_FORMAT_VERSION; f->SampleRate=48000; f->Channels=2; f->BitsPerSample=16; f->Format=PA2_AUDIO_FORMAT_PCM_S16; return sizeof(*f); }
PRODUCTION_EXPORT uint32_t FillAudioFrames(int16_t *samples, uint32_t frames) { for (uint32_t i=0;i<frames*2;++i) samples[i]=static_cast<int16_t>(200+i); return frames; }
