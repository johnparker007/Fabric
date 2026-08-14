#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <stddef.h>
_Static_assert(sizeof(((FabricCharacterDisplay *)0)->characters) ==
                   FABRIC_CHARACTER_CAPACITY * sizeof(uint32_t),
               "inline character ownership");
_Static_assert(sizeof(((FabricCharacterDisplay *)0)->brightness) == sizeof(float),
               "character display brightness type");
_Static_assert(offsetof(FabricCharacterDisplay, brightness) == 160,
               "character display brightness offset");
_Static_assert(sizeof(FabricCharacterDisplay) == 164,
               "character display ABI size");
_Static_assert(FABRIC_ABI_VERSION_CURRENT == UINT32_C(0x00040000),
               "current ABI version");
_Static_assert(sizeof(((FabricSegmentDisplay *)0)->segment_masks) ==
                   FABRIC_SEGMENT_DIGIT_CAPACITY * sizeof(uint64_t),
               "inline segment ownership");
_Static_assert(sizeof(FabricAmberSystem6ReelConfigurationV1) == 208,
               "Amber reel configuration layout");
_Static_assert(sizeof(FabricInput) == 88, "Fabric input ABI size");
_Static_assert(offsetof(FabricInput, numerical_index) == 72, "input index offset");
_Static_assert(offsetof(FabricInput, kind) == 76, "input kind offset");
_Static_assert(offsetof(FabricInput, active) == 80, "input active offset");
_Static_assert(offsetof(FabricInput, coin_channel) == 81, "coin channel offset");
_Static_assert(offsetof(FabricInput, coin_value) == 82, "coin value offset");
_Static_assert(FABRIC_CAPABILITY_COIN_INPUT == (UINT64_C(1) << 6),
               "coin capability bit");
_Static_assert(FABRIC_CAPABILITY_DOT_MATRIX_DISPLAYS == (UINT64_C(1) << 7),
               "dot-matrix capability bit");
_Static_assert(sizeof(((FabricDotMatrixDisplay *)0)->dots) ==
                   FABRIC_DOT_MATRIX_MAX_DOTS,
               "inline dot ownership");
_Static_assert(sizeof(FabricAmberSystem6CoinConfigurationV2) == 408,
               "Amber coin v2 configuration layout");
_Static_assert(offsetof(FabricAmberSystem6CoinConfigurationV2, coin_pulse_cycles) == 24,
               "Amber pulse-cycle offset");
_Static_assert(sizeof(FabricAmberSystem6ConfigurationV2) == 648,
               "Amber machine v2 configuration layout");
_Static_assert(sizeof(FabricAmberMpu5ReelConfigV1) == 20,
               "MPU5 reel entry layout");
_Static_assert(sizeof(FabricAmberMpu5ReelConfigurationV1) == 176,
               "MPU5 reel configuration layout");
_Static_assert(sizeof(FabricAmberMpu5CoinChannelConfigV1) == 20,
               "MPU5 coin entry layout");
_Static_assert(sizeof(FabricAmberMpu5CoinConfigurationV1) == 152,
               "MPU5 coin configuration layout");
_Static_assert(sizeof(FabricAmberMpu5OptionsV1) == 60,
               "MPU5 options layout");
_Static_assert(sizeof(FabricAmberMpu5ConfigurationV1) == 404,
               "MPU5 machine configuration layout");
_Static_assert(sizeof(FabricAmberM1ReelConfig) == 4, "M1 reel layout");
_Static_assert(sizeof(FabricAmberM1HopperConfig) == 44, "M1 hopper layout");
_Static_assert(sizeof(FabricAmberM1Config) == 148, "M1 configuration layout");
_Static_assert(offsetof(FabricAmberM1Config, hoppers) == 60, "M1 hopper offset");
_Static_assert(offsetof(FabricAmberM1HopperConfig, coins_in) == 16, "M1 hopper counter offset");
_Static_assert(offsetof(FabricAmberMpu5ConfigurationV1, reels) == 16, "MPU5 reels offset");
_Static_assert(offsetof(FabricAmberMpu5ConfigurationV1, coins) == 192, "MPU5 coins offset");
_Static_assert(offsetof(FabricAmberMpu5ConfigurationV1, options) == 344, "MPU5 options offset");
int main(void) {
  FabricRuntime *runtime = 0;
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT, &runtime) != FABRIC_OK)
    return 1;
  FabricDestroyRuntime(runtime);
  return 0;
}
