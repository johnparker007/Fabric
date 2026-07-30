#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <stddef.h>
_Static_assert(sizeof(((FabricCharacterDisplay *)0)->characters) ==
                   FABRIC_CHARACTER_CAPACITY * sizeof(uint32_t),
               "inline character ownership");
_Static_assert(sizeof(((FabricSegmentDisplay *)0)->segment_masks) ==
                   FABRIC_SEGMENT_DIGIT_CAPACITY * sizeof(uint64_t),
               "inline segment ownership");
_Static_assert(sizeof(FabricAmberReelConfigurationV1) == 208,
               "Amber reel configuration layout");
int main(void) {
  FabricRuntime *runtime = 0;
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_1, &runtime) != FABRIC_OK)
    return 1;
  FabricDestroyRuntime(runtime);
  return 0;
}
