#include "ProductionAmberAbi.h"
#include <cstring>
#ifdef _WIN32
#define X extern "C" __declspec(dllexport)
#else
#define X extern "C" __attribute__((visibility("default")))
#endif
static bool live, loaded; static uint64_t cycles; static PA2_OutputSnapshot snapshot;
X uint8_t Initialise(){live=true; loaded=false; cycles=0; return 1;}
X uint8_t Shutdown(){bool v=live; live=false; return v;}
X uint8_t Reset(){return live&&loaded;}
X int32_t Run(uint32_t n){cycles+=n; return 0;}
X uint32_t LoadROM(uint8_t*a,uint8_t*,uint8_t*,uint8_t*){loaded=a; return loaded;}
X uint32_t LoadSoundROM(uint8_t*a,uint8_t*,uint8_t*,uint8_t*){return a?1:0;}
X uint32_t GetOutputSnapshotSize(){return sizeof(snapshot);}
X uint32_t GetOutputSnapshot(void*out,uint32_t n){if(n<sizeof(snapshot))return 0; snapshot={}; snapshot.SizeBytes=sizeof(snapshot); snapshot.Version=PA2_OUTPUT_SNAPSHOT_VERSION; snapshot.MatrixLampCount=256; snapshot.TriacLampCount=8; snapshot.ReelCount=6; snapshot.AlphaSegmentedDisplayCount=1; snapshot.AlphaSegmented[0].SegmentCount=16; snapshot.ElectronicMechCount=1; snapshot.MeterCount=6; snapshot.DipCount=16; snapshot.HopperCount=2; std::memcpy(out,&snapshot,sizeof(snapshot)); return sizeof(snapshot);}
X void TurnSwitchOn(uint8_t){} X void TurnSwitchOff(uint8_t){}
X uint8_t CoinIn(uint8_t,uint8_t coin,uint8_t){return coin<6;}
X uint32_t GetAudioFormat(PA2_AudioFormat*out,uint32_t n){if(n<sizeof(*out))return 0; *out={sizeof(*out),PA2_AUDIO_FORMAT_VERSION,48000,2,16,PA2_AUDIO_FORMAT_PCM_S16}; return sizeof(*out);}
X uint32_t FillAudioFrames(int16_t*out,uint32_t frames){std::memset(out,0,frames*2*sizeof(*out));return frames;}
X void SetSteps(uint8_t,uint8_t){} X void SetOptoStart(uint8_t,uint8_t){} X void SetOptoEnd(uint8_t,uint8_t){} X void SetOptoInvert(uint8_t,uint8_t){} X void SetDIP(uint8_t,uint8_t){} X void SetPercent(uint8_t){} X void SetEDCEnable(uint8_t){}
X void SetHopperEnable(uint8_t,uint8_t){}
X void SetHopperOptoEnable(uint8_t,uint8_t){}
X void SetHopperOptoReturn(uint8_t,uint8_t){}
X void SetHopperMotorEnable(uint8_t,uint8_t){}
X void SetHopperCoin(uint8_t,uint8_t){}
X void SetHopperLoEnable(uint8_t,uint8_t){}
X void SetHopperLoInvert(uint8_t,uint8_t){}
X void SetHopperLoSwitch(uint8_t,uint8_t){}
X void SetHopperHiEnable(uint8_t,uint8_t){}
X void SetHopperHiInvert(uint8_t,uint8_t){}
X void SetHopperHiSwitch(uint8_t,uint8_t){}
X void SetHopperLoIndicator(uint8_t,uint8_t){}
X void SetHopperHiIndicator(uint8_t,uint8_t){}
X void SetHopperCoinsIn(uint8_t,uint32_t){}
X void SetHopperCoinsOut(uint8_t,uint32_t){}
X void SetHopperLevel(uint8_t,uint32_t){}
X void SetHopperFullLevel(uint8_t,uint32_t){}
X void SetHopperLoLevel(uint8_t,uint32_t){}
X void SetHopperHiLevel(uint8_t,uint32_t){}
X void SetHopperCoinsRefilled(uint8_t,uint32_t){}
