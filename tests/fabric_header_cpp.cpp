#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"
#include <type_traits>
static_assert(std::is_standard_layout<FabricMachineSnapshot>::value, "C ABI snapshot");
static_assert(std::is_standard_layout<FabricRomResource>::value, "C ABI resource");
static_assert(sizeof(FabricAmberCoinConfigurationV1) == 408, "Amber coin configuration layout");
int main() {
  FabricRuntime *runtime = nullptr;
  if (FabricCreateRuntime(FABRIC_ABI_VERSION_1, &runtime) != FABRIC_OK)
    return 1;
  FabricDestroyRuntime(runtime);
  return 0;
}
