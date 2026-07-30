#include "FabricRuntimeInternal.h"
#include "FakeBackend.h"

#include <cmath>
#include <cstring>
#include <iostream>

namespace {
int failures = 0;
#define CHECK(condition) do { if (!(condition)) { \
    std::cerr << __FILE__ << ':' << __LINE__ << ": " #condition " failed\n"; ++failures; } } while (false)

FabricLaunchRequest request(const char *kind, const char *machine, const char *path) {
    FabricLaunchRequest value{};
    value.struct_size = sizeof(value);
    value.struct_version = FABRIC_ABI_VERSION_1;
    std::strncpy(value.backend_kind, kind, sizeof(value.backend_kind) - 1);
    std::strncpy(value.machine_identifier, machine, sizeof(value.machine_identifier) - 1);
    std::strncpy(value.backend_path, path, sizeof(value.backend_path) - 1);
    return value;
}
}

int main() {
    FabricRuntime *runtime = nullptr;
    CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_1, &runtime) == FABRIC_OK);
    auto first = std::make_shared<FakeBackendState>();
    auto second = std::make_shared<FakeBackendState>();
    const uint64_t unknown_capability = UINT64_C(1) << 63;
    CHECK(FabricRegisterBackendProvider(runtime, MakeFakeProvider("fake-a", "machine-a", first)) == FABRIC_OK);
    CHECK(FabricRegisterBackendProvider(runtime, MakeFakeProvider(
        "fake-b", "machine-b", second, FABRIC_CAPABILITY_LAMPS | unknown_capability)) == FABRIC_OK);

    FabricMachineSession *missing = nullptr;
    auto missing_request = request("fake-a", "machine-b", "/not/selected");
    CHECK(FabricCreateSession(runtime, &missing_request, &missing) == FABRIC_NOT_FOUND);
    char runtime_error[512]{}; uint32_t runtime_required=0;
    CHECK(FabricRuntimeGetLastError(runtime, runtime_error, sizeof(runtime_error), &runtime_required) == FABRIC_OK);
    CHECK(std::strstr(runtime_error, "no backend provider") != nullptr);

    FabricMachineSession *session = nullptr;
    auto selected = request("fake-b", "machine-b", "/absolute/backend/library.dll");
    CHECK(FabricCreateSession(runtime, &selected, &session) == FABRIC_OK);
    CHECK(first->created == 0 && second->created == 1);
    CHECK(second->received_backend_path == "/absolute/backend/library.dll");

    FabricCapabilities capabilities{sizeof(FabricCapabilities), FABRIC_ABI_VERSION_1, 0, {0}};
    CHECK(FabricSessionGetCapabilities(session, &capabilities) == FABRIC_OK);
    CHECK((capabilities.flags & unknown_capability) != 0); // Unknown bits cross the boundary unchanged.
    CHECK(FabricSessionInitialise(session) == FABRIC_OK);
    CHECK(FabricSessionReset(session) == FABRIC_OK);
    CHECK(FabricSessionAdvance(session, 1000000) == FABRIC_OK);

    FabricLamp lamp{};
    FabricMachineSnapshot snapshot{};
    snapshot.struct_size = sizeof(snapshot);
    snapshot.struct_version = FABRIC_ABI_VERSION_1;
    snapshot.lamps = &lamp;
    snapshot.lamp_capacity = 1;
    CHECK(FabricSessionGetSnapshot(session, &snapshot) == FABRIC_OK);
    CHECK(snapshot.sequence == 42 && snapshot.lamp_count == 1);
    CHECK(std::strcmp(lamp.identifier, "credit") == 0 && lamp.numerical_index == 7);
    CHECK(lamp.logical_state == 1 && std::fabs(lamp.brightness - 0.625f) < 0.001f);

    second->fail_advance = true;
    CHECK(FabricSessionAdvance(session, 1000000) == FABRIC_BACKEND_ERROR);
    char error[64]{};
    uint32_t required = 0;
    CHECK(FabricSessionGetLastError(session, error, sizeof(error), &required) == FABRIC_OK);
    CHECK(std::strcmp(error, "fake advance failure") == 0 && required == 21);
    CHECK(FabricSessionShutdown(session) == FABRIC_OK);
    FabricDestroySession(session);
    CHECK(second->initialised == 1 && second->reset == 1 && second->advanced == 2);
    CHECK(second->shutdown == 1 && second->destroyed == 1);
    FabricDestroyRuntime(runtime);
    return failures ? 1 : 0;
}
