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
_Static_assert(FABRIC_ABI_VERSION_CURRENT == UINT32_C(0x00020000),
               "current ABI version");
_Static_assert(sizeof(((FabricSegmentDisplay *)0)->segment_masks) ==
                   FABRIC_SEGMENT_DIGIT_CAPACITY * sizeof(uint64_t),
               "inline segment ownership");
_Static_assert(sizeof(FabricAmberReelConfigurationV1) == 208,
               "Amber reel configuration layout");
_Static_assert(sizeof(FabricAmberCoinChannelConfigV1) == 20,
               "Amber coin channel layout");
_Static_assert(offsetof(FabricAmberCoinChannelConfigV1, lockout_value) == 12,
               "Amber lockout value offset");
_Static_assert(offsetof(FabricAmberCoinChannelConfigV1, lockout_invert) == 16,
               "Amber lockout invert offset");
_Static_assert(sizeof(FabricAmberCoinConfigurationV1) == 408,
               "Amber coin configuration layout");
_Static_assert(sizeof(FabricAmberConfigurationV1) == 648,
               "top-level Amber configuration layout");
int main(void) {
  FabricRuntime *runtime = 0;
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT, &runtime) != FABRIC_OK)
    return 1;
  FabricDestroyRuntime(runtime);
  return 0;
}
