#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#define CHECK(x) do { if (!(x)) { std::cerr << "check failed line " << __LINE__ << ": " #x "\n"; return 1; } } while (0)
int main(){
  const char *rom="fabric-sc4-test.rom"; {std::ofstream f(rom,std::ios::binary); f.put('x');}
  const char *roms[]={rom}; FabricRuntime *runtime=nullptr; FabricMachineSession *session=nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT,&runtime)==FABRIC_OK);
  FabricLaunchRequest r{}; r.struct_size=sizeof(r); r.struct_version=FABRIC_ABI_VERSION_CURRENT;
  std::strcpy(r.backend_kind,"amber"); std::strcpy(r.machine_identifier,"bellfruit-scorpion4");
  std::strncpy(r.backend_path,FAKE_PRODUCTION_AMBER_SC4_PATH,sizeof(r.backend_path)-1); r.rom_paths=roms; r.rom_path_count=1;
  FabricAmberScorpion4Config config{}; config.magic=FABRIC_AMBER_SCORPION4_CONFIGURATION_MAGIC;
  config.struct_size=sizeof(config); config.version=FABRIC_AMBER_SCORPION4_CONFIGURATION_VERSION_1;
  config.reel_count=6; config.hopper_count=2; config.coin_channel_count=6;
  for(auto &reel:config.reels) reel.steps=96;
  r.machine_configuration=&config; r.machine_configuration_size=sizeof(config);
  FabricResult created=FabricCreateSession(runtime,&r,&session); if(created!=FABRIC_OK){char e[512]{};uint32_t n=0;FabricRuntimeGetLastError(runtime,e,sizeof(e),&n);std::cerr<<e<<"\n";} CHECK(created==FABRIC_OK); CHECK(FabricSessionInitialise(session)==FABRIC_OK);
  FabricCapabilities cap{sizeof(cap),FABRIC_ABI_VERSION_CURRENT}; CHECK(FabricSessionGetCapabilities(session,&cap)==FABRIC_OK);
  CHECK((cap.flags&FABRIC_CAPABILITY_DOT_MATRIX_DISPLAYS)!=0);
  FabricLamp lamps[256]{}; FabricReel reels[6]{}; FabricCharacterDisplay chars[2]{}; FabricSegmentDisplay segs[32]{}; FabricDotMatrixDisplay dot[1]{};
  FabricMachineSnapshot s{}; s.struct_size=sizeof(s); s.struct_version=FABRIC_ABI_VERSION_CURRENT; s.lamps=lamps;s.lamp_capacity=256;s.reels=reels;s.reel_capacity=6;s.character_displays=chars;s.character_display_capacity=2;s.segment_displays=segs;s.segment_display_capacity=32;
  CHECK(FabricSessionGetSnapshot(session,&s)==FABRIC_BUFFER_TOO_SMALL); CHECK(s.dot_matrix_display_count==1);
  s.dot_matrix_display_capacity=1; CHECK(FabricSessionGetSnapshot(session,&s)==FABRIC_INVALID_ARGUMENT);
  s.dot_matrix_displays=dot; CHECK(FabricSessionGetSnapshot(session,&s)==FABRIC_OK);
  CHECK(s.lamp_count==256&&s.reel_count==6&&s.character_display_count==2&&s.segment_display_count==32&&s.dot_matrix_display_count==1);
  CHECK(dot[0].struct_size==sizeof(dot[0])&&dot[0].struct_version==FABRIC_ABI_VERSION_CURRENT);
  CHECK(std::strcmp(dot[0].identifier,"amber.dot-matrix.0")==0&&dot[0].width==96&&dot[0].height==8&&dot[0].dot_count==768&&dot[0].dot_capacity==FABRIC_DOT_MATRIX_MAX_DOTS);
  CHECK(dot[0].dots[0*96+0]==1&&dot[0].dots[7*96+0]==1); CHECK(dot[0].dots[2*96+4]==1&&dot[0].dots[5*96+4]==1);
  CHECK(dot[0].dots[1*96+6]==1); for(unsigned y=0;y<8;++y) CHECK(dot[0].dots[y*96+5]==0);
  for(unsigned i=0;i<FABRIC_DOT_MATRIX_MAX_DOTS;++i) CHECK(dot[0].dots[i]<=1);
  CHECK(dot[0].brightness==.625f&&chars[0].characters[0]==0x1234&&chars[1].characters[0]==0x4321);
  FabricDestroySession(session); FabricDestroyRuntime(runtime); std::remove(rom); return 0;
}
