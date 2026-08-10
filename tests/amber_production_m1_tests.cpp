#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <cstdio>
#include <cstring>
#define CHECK(x) do { if (!(x)) return __LINE__; } while (0)
int main() {
  const char *rom = "fabric-m1-test.rom"; FILE *f = std::fopen(rom, "wb");
  CHECK(f); std::fputc(0, f); std::fclose(f);
  FabricAmberM1Config c{}; c.magic=FABRIC_AMBER_M1_CONFIGURATION_MAGIC;
  c.struct_size=sizeof(c); c.version=1; c.reel_count=6; c.hopper_count=2;
  for(auto &r:c.reels){r.steps=96;r.opto_end=4;}
  c.hoppers[0].enabled=c.hoppers[1].enabled=1;
  c.hoppers[0].opto_enable=c.hoppers[1].opto_enable=1;
  c.hoppers[0].motor_enable=c.hoppers[1].motor_enable=1;
  FabricLaunchRequest q{}; q.struct_size=sizeof(q); q.struct_version=FABRIC_ABI_VERSION_CURRENT;
  std::strcpy(q.backend_kind,"amber"); std::strcpy(q.machine_identifier,"maygay-m1");
  std::strncpy(q.backend_path,FAKE_PRODUCTION_AMBER_M1_PATH,sizeof(q.backend_path)-1);
  q.rom_paths=&rom; q.rom_path_count=1; q.machine_configuration=&c; q.machine_configuration_size=sizeof(c);
  FabricRuntime *runtime=nullptr; FabricMachineSession *session=nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT,&runtime)==FABRIC_OK);
  CHECK(FabricCreateSession(runtime,&q,&session)==FABRIC_OK);
  CHECK(FabricSessionInitialise(session)==FABRIC_OK);
  CHECK(FabricSessionAdvance(session,1000000)==FABRIC_OK);
  FabricAudioFormat af{}; af.struct_size=sizeof(af); af.struct_version=FABRIC_ABI_VERSION_CURRENT;
  CHECK(FabricSessionGetAudioFormat(session,&af)==FABRIC_OK); CHECK(af.sample_rate==48000&&af.channel_count==2&&af.bits_per_sample==16);
  FabricLamp lamps[256]{}; FabricReel reels[6]{}; FabricCharacterDisplay alpha[1]{};
  FabricMachineSnapshot s{}; s.struct_size=sizeof(s); s.struct_version=FABRIC_ABI_VERSION_CURRENT;
  s.lamps=lamps;s.lamp_capacity=256;s.reels=reels;s.reel_capacity=6;s.character_displays=alpha;s.character_display_capacity=1;
  CHECK(FabricSessionGetSnapshot(session,&s)==FABRIC_OK); CHECK(s.lamp_count==256&&s.reel_count==6&&s.character_display_count==1&&s.segment_display_count==0);
  FabricInput in{}; in.struct_size=sizeof(in);in.struct_version=FABRIC_ABI_VERSION_CURRENT;in.kind=FABRIC_INPUT_COIN;in.active=1;in.coin_channel=2;in.coin_value=7;
  CHECK(FabricSessionSubmitInput(session,&in)==FABRIC_OK); CHECK(FabricSessionSubmitInput(session,&in)==FABRIC_OK); in.active=0; CHECK(FabricSessionSubmitInput(session,&in)==FABRIC_OK);
  CHECK(FabricSessionReset(session)==FABRIC_OK); CHECK(FabricSessionShutdown(session)==FABRIC_OK);
  FabricDestroySession(session); FabricDestroyRuntime(runtime); std::remove(rom); return 0;
}
