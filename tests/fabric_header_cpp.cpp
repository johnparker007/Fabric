#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <cstddef>
#include <type_traits>
static_assert(std::is_standard_layout<FabricMachineSnapshot>::value, "C ABI snapshot");
static_assert(std::is_standard_layout<FabricRomResource>::value, "C ABI resource");
static_assert(std::is_same<decltype(FabricCharacterDisplay::brightness), float>::value,
              "character brightness is a float");
static_assert(sizeof(FabricAmberCoinConfigurationV1) == 408, "Amber coin configuration layout");
static_assert(sizeof(FabricAmberCoinChannelConfigV1) == 20, "Amber coin channel layout");
static_assert(offsetof(FabricAmberCoinChannelConfigV1, lockout_value) == 12,
              "Amber lockout value offset");
static_assert(offsetof(FabricAmberCoinChannelConfigV1, lockout_invert) == 16,
              "Amber lockout invert offset");
static_assert(sizeof(FabricAmberConfigurationV1) == 648,
              "top-level Amber configuration layout");
int main() {
  FabricRuntime *runtime = nullptr;
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT, &runtime) != FABRIC_OK)
    return 1;
  FabricDestroyRuntime(runtime);
  return 0;
}
