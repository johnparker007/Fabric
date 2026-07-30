#ifndef FABRIC_RUNTIME_INTERNAL_H
#define FABRIC_RUNTIME_INTERNAL_H

#include "FabricBackend.h"
#include <memory>
#include <vector>

namespace fabric {

class RuntimeRegistry {
public:
    FabricResult register_provider(std::unique_ptr<FabricBackendProvider> provider) noexcept;
    FabricResult create(const FabricLaunchRequest &request,
                        std::unique_ptr<FabricBackendInstance> &instance,
                        std::string &error) const noexcept;
    size_t provider_count() const noexcept { return providers_.size(); }

private:
    std::vector<std::unique_ptr<FabricBackendProvider>> providers_;
};

} // namespace fabric

/* Composition root hook: adapters are registered explicitly, not discovered globally. */
FabricResult FabricRegisterBackendProvider(FabricRuntime *runtime,
                                           std::unique_ptr<fabric::FabricBackendProvider> provider) noexcept;

namespace fabric { std::unique_ptr<FabricBackendProvider> MakeAmberBackendProvider(); }
#endif
