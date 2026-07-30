#ifndef FABRIC_BACKEND_H
#define FABRIC_BACKEND_H

#include "fabric/fabric.h"

#include <memory>
#include <string>

namespace fabric {

class FabricBackendInstance {
public:
    virtual ~FabricBackendInstance() = default;
    virtual FabricResult initialise() noexcept = 0;
    virtual FabricResult reset() noexcept = 0;
    virtual FabricResult advance(uint64_t elapsed_nanoseconds) noexcept = 0;
    virtual FabricResult shutdown() noexcept = 0;
    virtual FabricResult submit_input(const FabricInput &input) noexcept = 0;
    virtual FabricResult capabilities(FabricCapabilities &capabilities) noexcept = 0;
    virtual FabricResult snapshot(FabricMachineSnapshot &snapshot) noexcept = 0;
    virtual FabricResult audio_format(FabricAudioFormat &format) noexcept = 0;
    virtual FabricResult read_audio(int16_t *samples, uint32_t frame_capacity,
                                    uint32_t &frames_written) noexcept = 0;
    virtual std::string last_error() const noexcept = 0;
};

class FabricBackendProvider {
public:
    virtual ~FabricBackendProvider() = default;
    virtual bool supports(const std::string &backend_kind,
                          const std::string &machine_identifier) const noexcept = 0;
    virtual FabricResult create(const FabricLaunchRequest &request,
                                std::unique_ptr<FabricBackendInstance> &instance,
                                std::string &error) noexcept = 0;
};

} // namespace fabric
#endif
