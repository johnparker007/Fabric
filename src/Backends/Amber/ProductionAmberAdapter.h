#ifndef FABRIC_AMBER_PRODUCTION_ADAPTER_H
#define FABRIC_AMBER_PRODUCTION_ADAPTER_H

#include "FabricBackend.h"

#include <memory>
#include <string>

namespace fabric {
class AmberDynamicLibrary;
FabricResult CreateProductionAmberInstance(
    const FabricLaunchRequest &request,
    std::unique_ptr<AmberDynamicLibrary> library,
    std::unique_ptr<FabricBackendInstance> &out, std::string &error) noexcept;
}
#endif
