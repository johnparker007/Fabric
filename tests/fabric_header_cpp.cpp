#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <type_traits>
static_assert(std::is_standard_layout<FabricMachineSnapshot>::value, "C ABI snapshot");
static_assert(std::is_standard_layout<FabricRomResource>::value, "C ABI resource");
static_assert(std::is_same<decltype(FabricCharacterDisplay::brightness), float>::value,
              "character brightness is a float");
static_assert(sizeof(FabricAmberSystem6CoinConfigurationV2) == 408, "Amber coin configuration layout");
static_assert(std::is_standard_layout<FabricAmberMpu5ConfigurationV1>::value,
              "MPU5 configuration is a C ABI layout");
static_assert(sizeof(FabricAmberMpu5ReelConfigV1) == 20,
              "MPU5 reel entry layout");
static_assert(sizeof(FabricAmberMpu5ReelConfigurationV1) == 176,
              "MPU5 reel configuration layout");
static_assert(sizeof(FabricAmberMpu5CoinChannelConfigV1) == 20,
              "MPU5 coin entry layout");
static_assert(sizeof(FabricAmberMpu5CoinConfigurationV1) == 152,
              "MPU5 coin configuration layout");
static_assert(sizeof(FabricAmberMpu5OptionsV1) == 60, "MPU5 options layout");
static_assert(offsetof(FabricAmberMpu5ConfigurationV1, reels) == 16, "MPU5 reels offset");
static_assert(offsetof(FabricAmberMpu5ConfigurationV1, coins) == 192, "MPU5 coins offset");
static_assert(offsetof(FabricAmberMpu5ConfigurationV1, options) == 344, "MPU5 options offset");
static_assert(sizeof(FabricAmberMpu5ConfigurationV1) == 404,
              "MPU5 configuration layout");
int main() {
  FabricRuntime *runtime = nullptr;
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT, &runtime) != FABRIC_OK)
    return 1;
  FabricDestroyRuntime(runtime);
  return 0;
}
