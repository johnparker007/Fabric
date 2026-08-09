#include "ProductionAmberAbi.h"
#include <cstring>

#ifdef _WIN32
#define EXPORT extern "C" __declspec(dllexport)
#else
#define EXPORT extern "C" __attribute__((visibility("default")))
#endif
namespace {
bool live; uint32_t cycles, resets, reel_calls, dip_calls, coin_calls, snapshot_calls;
uint8_t reels[4], opto_start[4], opto_end[4], invert[4], dips[16], switches[256];
uint32_t rom_size, rom_address, rom_checksum;
}
EXPORT uint8_t Initialise() { live=true; cycles=resets=reel_calls=dip_calls=coin_calls=0; return 1; }
EXPORT void Shutdown() { live=false; }
EXPORT uint8_t Reset(int, int, int) { ++resets; std::memset(reels,0,sizeof(reels)); std::memset(dips,0,sizeof(dips)); return 1; }
EXPORT INT32 Run(INT32 n) { cycles += static_cast<uint32_t>(n); return n; }
EXPORT uint8_t LoadROM(uint8_t *p, INT32 n, INT32 address) {
  rom_size=static_cast<uint32_t>(n); rom_address=static_cast<uint32_t>(address); rom_checksum=0;
  for (INT32 i=0;i<n;++i) rom_checksum += p[i]; return live && n>0;
}
#ifndef FAKE_AMBER_OMIT_SET_STEPS
EXPORT void SetSteps(uint8_t i,uint8_t n) { if(i<4)reels[i]=n; ++reel_calls; }
#endif
EXPORT void SetOptoStart(uint8_t i,uint8_t n) { if(i<4)opto_start[i]=n; ++reel_calls; }
EXPORT void SetOptoEnd(uint8_t i,uint8_t n) { if(i<4)opto_end[i]=n; ++reel_calls; }
EXPORT void SetOptoInvert(uint8_t i,uint8_t n) { if(i<4)invert[i]=n; ++reel_calls; }
EXPORT uint8_t SetDIP(uint8_t i,bool v) { if(i>=16)return 0; dips[i]=v; ++dip_calls; return 1; }
EXPORT void TurnSwitchOn(uint8_t i) { switches[i]=1; }
EXPORT void TurnSwitchOff(uint8_t i) { switches[i]=0; }
EXPORT void CoinIn(int,int) { ++coin_calls; }
EXPORT uint32_t GetOutputSnapshotSize() { return sizeof(PA2_OutputSnapshot); }
EXPORT uint32_t GetOutputSnapshot(void *buffer,uint32_t size) {
  if (!buffer || size < sizeof(PA2_OutputSnapshot)) return 0;
  auto *s=static_cast<PA2_OutputSnapshot *>(buffer); ++snapshot_calls;
  std::memset(s,0,sizeof(*s)); s->SizeBytes=sizeof(*s); s->Version=PA2_OUTPUT_SNAPSHOT_VERSION;
  s->MatrixLampCount=32; s->ReelCount=4; s->AlphaSegmentedDisplayCount=1; s->LedDisplayCount=2;
  s->TriacLampCount=1; s->MeterCount=6;
  s->MatrixLamps[0].OnOff=switches[7]; s->MatrixLamps[0].Brightness=switches[7]?1.f:0.f;
  s->MatrixLamps[1].Brightness=static_cast<float>(cycles); s->MatrixLamps[2].Brightness=static_cast<float>(rom_size);
  s->MatrixLamps[3].Brightness=static_cast<float>(rom_address); s->MatrixLamps[4].Brightness=static_cast<float>(rom_checksum);
  s->MatrixLamps[5].Brightness=static_cast<float>(reel_calls); s->MatrixLamps[6].Brightness=static_cast<float>(dip_calls);
  s->MatrixLamps[7].Brightness=static_cast<float>(coin_calls); s->MatrixLamps[8].Brightness=switches[3]?1.f:0.f;
  s->MatrixLamps[9].Brightness=static_cast<float>(opto_start[2]+opto_end[2]+invert[2]);
  s->MatrixLamps[10].Brightness=static_cast<float>(snapshot_calls);
  for(int i=0;i<4;++i)s->Reels[i].Position=10+i;
  s->AlphaSegmented[0].SegmentCount=PA2_ALPHA_SEGMENTS_MPU4; s->AlphaSegmented[0].Segments[0]=0x1234; s->AlphaSegmented[0].Brightness=1.f;
  s->LedDisplays[0].OnOff=0x5a; s->LedDisplays[0].Brightness=1.f;
  return sizeof(*s);
}
EXPORT uint32_t GetAudioFormat(PA2_AudioFormat *f,uint32_t size) {
  if (!f || size < sizeof(*f)) return 0; f->SizeBytes=sizeof(*f); f->Version=PA2_AUDIO_FORMAT_VERSION;
  f->SampleRate=48000; f->Channels=1; f->BitsPerSample=16; f->Format=PA2_AUDIO_FORMAT_PCM_S16; return sizeof(*f);
}
#ifndef FAKE_AMBER_OMIT_FILL_AUDIO
EXPORT uint32_t FillAudioFrames(int16_t *samples,uint32_t frames) {
  for(uint32_t i=0;i<frames;++i)samples[i]=static_cast<int16_t>(i+1); return frames;
}
#endif
