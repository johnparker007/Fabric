#include "FakeBackend.h"
#include <algorithm>
#include <cstring>
#include <utility>

namespace {
class FakeInstance final : public fabric::FabricBackendInstance {
public:
    FakeInstance(std::shared_ptr<FakeBackendState> state, uint64_t flags)
        : state_(std::move(state)), flags_(flags) {}
    ~FakeInstance() override { ++state_->destroyed; }
    FabricResult initialise() noexcept override { ++state_->initialised; return FABRIC_OK; }
    FabricResult reset() noexcept override { ++state_->reset; return FABRIC_OK; }
    FabricResult advance(uint64_t) noexcept override {
        ++state_->advanced;
        if (state_->fail_advance) { error_ = "fake advance failure"; return FABRIC_BACKEND_ERROR; }
        return FABRIC_OK;
    }
    FabricResult shutdown() noexcept override { ++state_->shutdown; return FABRIC_OK; }
    FabricResult submit_input(const FabricInput &) noexcept override { return FABRIC_OK; }
    FabricResult capabilities(FabricCapabilities &value) noexcept override {
        value.flags = flags_; return FABRIC_OK;
    }
    FabricResult snapshot(FabricMachineSnapshot &value) noexcept override {
        value.sequence = 42;
        value.lamp_count = 1;
        value.reel_count = value.character_display_count = value.segment_display_count = 0;
        if (value.lamp_capacity < 1 || !value.lamps) return FABRIC_BUFFER_TOO_SMALL;
        FabricLamp lamp{};
        lamp.struct_size = sizeof(lamp);
        lamp.struct_version = FABRIC_ABI_VERSION_1;
        std::strcpy(lamp.identifier, "credit");
        lamp.numerical_index = 7;
        lamp.logical_state = 1;
        lamp.brightness = 0.625f;
        value.lamps[0] = lamp;
        return FABRIC_OK;
    }
    FabricResult audio_format(FabricAudioFormat &) noexcept override { return FABRIC_NOT_SUPPORTED; }
    FabricResult read_audio(int16_t *, uint32_t, uint32_t &written) noexcept override {
        written = 0; return FABRIC_NOT_SUPPORTED;
    }
    std::string last_error() const noexcept override { return error_; }
private:
    std::shared_ptr<FakeBackendState> state_;
    uint64_t flags_;
    std::string error_;
};

class FakeProvider final : public fabric::FabricBackendProvider {
public:
    FakeProvider(std::string kind, std::string machine, std::shared_ptr<FakeBackendState> state, uint64_t flags)
        : kind_(std::move(kind)), machine_(std::move(machine)), state_(std::move(state)), flags_(flags) {}
    bool supports(const std::string &kind, const std::string &machine) const noexcept override {
        return kind == kind_ && machine == machine_;
    }
    FabricResult create(const FabricLaunchRequest &request,
                        std::unique_ptr<fabric::FabricBackendInstance> &instance,
                        std::string &) noexcept override {
        ++state_->created;
        state_->received_backend_path = request.backend_path;
        try { instance = std::make_unique<FakeInstance>(state_, flags_); }
        catch (...) { return FABRIC_INTERNAL_ERROR; }
        return FABRIC_OK;
    }
private:
    std::string kind_;
    std::string machine_;
    std::shared_ptr<FakeBackendState> state_;
    uint64_t flags_;
};
}

std::unique_ptr<fabric::FabricBackendProvider> MakeFakeProvider(
    const std::string &kind, const std::string &machine, std::shared_ptr<FakeBackendState> state,
    uint64_t flags) {
    return std::make_unique<FakeProvider>(kind, machine, std::move(state), flags);
}
