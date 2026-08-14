#include "ProductionAmberAbi.h"
#include <cstring>
#ifdef _WIN32
#define X extern "C" __declspec(dllexport)
#else
#define X extern "C" __attribute__((visibility("default")))
#endif
static bool live, loaded;
static PA2_OutputSnapshot snapshot;
X uint8_t Initialise(){live=true; loaded=false; return 1;}
X uint8_t Shutdown(){bool v=live; live=false; return v;}
X uint8_t Reset(){return live&&loaded;}
X int32_t Run(uint32_t){return 0;}
X uint32_t LoadROM(uint8_t*a,uint8_t*,uint8_t*,uint8_t*){loaded=a; return loaded;}
X uint32_t LoadSoundROM(uint8_t*a,uint8_t*,uint8_t*,uint8_t*){return a?1:0;}
X uint32_t GetOutputSnapshotSize(){return sizeof(snapshot);}
X uint32_t GetOutputSnapshot(void*out,uint32_t n){
  if(n<sizeof(snapshot))return 0; snapshot={}; snapshot.SizeBytes=sizeof(snapshot);
  snapshot.Version=PA2_OUTPUT_SNAPSHOT_VERSION; snapshot.MatrixLampCount=256;
  snapshot.ReelCount=6; snapshot.AlphaSegmentedDisplayCount=2;
  snapshot.AlphaSegmented[0].SegmentCount=16; snapshot.AlphaSegmented[1].SegmentCount=14;
  snapshot.AlphaSegmented[0].Segments[0]=0x1234; snapshot.AlphaSegmented[0].Brightness=.25f;
  snapshot.AlphaSegmented[1].Segments[0]=0x4321; snapshot.AlphaSegmented[1].Brightness=.75f;
  snapshot.AlphaDotDisplayCount=1; snapshot.AlphaDot[0].Columns[0][0]=0x81;
  snapshot.AlphaDot[0].Columns[0][4]=0x24; snapshot.AlphaDot[0].Columns[1][0]=0x02;
  snapshot.AlphaDot[0].Brightness=.625f; snapshot.LedDisplayCount=32;
  snapshot.MeterCount=6; snapshot.DipCount=16; snapshot.HopperCount=2;
  std::memcpy(out,&snapshot,sizeof(snapshot)); return sizeof(snapshot);
}
X void TurnSwitchOn(uint8_t){} X void TurnSwitchOff(uint8_t){}
X uint8_t CoinIn(uint8_t,uint8_t,uint8_t){return 1;}
X uint32_t GetAudioFormat(PA2_AudioFormat*out,uint32_t n){if(n<sizeof(*out))return 0; *out={sizeof(*out),PA2_AUDIO_FORMAT_VERSION,48000,2,16,PA2_AUDIO_FORMAT_PCM_S16}; return sizeof(*out);}
X uint32_t FillAudioFrames(int16_t*out,uint32_t frames){std::memset(out,0,frames*2*sizeof(*out));return frames;}
#define V(name) X void name(uint8_t){}
#define VV(name) X void name(uint8_t,uint8_t){}
#define VI(name) X void name(uint8_t,uint32_t){}
VV(SetSteps) VV(SetOptoStart) VV(SetOptoEnd) VV(SetOptoInvert) VV(SetDIP)
V(SetStake) V(SetPrize) V(SetPercent) V(SetEDCEnable) VV(SetCoinValue) VV(SetCoinEnable) V(SetHopperType)
VV(SetHopperEnable) VI(SetHopperCoinsIn) VI(SetHopperCoinsOut) VV(SetHopperCoin)
VI(SetHopperLevel) VI(SetHopperFullLevel) VV(SetHopperLoEnable) VI(SetHopperLoLevel)
VV(SetHopperHiEnable) VI(SetHopperHiLevel) VI(SetHopperCoinsRefilled)
