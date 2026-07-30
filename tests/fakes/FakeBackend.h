#ifndef FABRIC_TEST_FAKE_BACKEND_H
#define FABRIC_TEST_FAKE_BACKEND_H

#include "FabricBackend.h"
#include <memory>

struct FakeBackendState {
    unsigned created = 0;
    unsigned initialised = 0;
    unsigned reset = 0;
    unsigned advanced = 0;
    unsigned shutdown = 0;
    unsigned destroyed = 0;
    bool fail_advance = false;
    std::string received_backend_path;
};

std::unique_ptr<fabric::FabricBackendProvider> MakeFakeProvider(
    const std::string &kind, const std::string &machine, std::shared_ptr<FakeBackendState> state,
    uint64_t capability_flags = FABRIC_CAPABILITY_LAMPS);

#endif
