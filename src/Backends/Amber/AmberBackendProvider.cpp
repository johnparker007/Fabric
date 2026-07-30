#include "AmberDynamicLibrary.h"
#include "AmberTrace.h"
#include "FabricBackend.h"
#include "ProductionAmberAdapter.h"
#include "fabric/fabric_amber.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace fabric {
namespace {
using Rom = std::pair<uint32_t, std::string>;

FabricResult validate_roms(const FabricLaunchRequest &request,
                           std::string &error) {
  if (request.rom_path_count && request.rom_resource_count) {
    error = "untyped and typed ROM lists cannot be supplied together";
    return FABRIC_INVALID_ARGUMENT;
  }
  if (request.rom_path_count > 4) {
    error = "Amber supports at most four program ROM paths";
    return FABRIC_INVALID_ARGUMENT;
  }
  for (uint32_t i = 0; i < request.rom_path_count; ++i)
    if (!request.rom_paths || !request.rom_paths[i] || !request.rom_paths[i][0]) {
      error = "program ROM path is empty";
      return FABRIC_INVALID_ARGUMENT;
    }
  std::set<std::pair<uint32_t, uint32_t>> seen;
  std::vector<Rom> program, sound;
  for (uint32_t i = 0; i < request.rom_resource_count; ++i) {
    if (!request.rom_resources) {
      error = "typed ROM list is null";
      return FABRIC_INVALID_ARGUMENT;
    }
    const auto &resource = request.rom_resources[i];
    if (resource.struct_size < sizeof(resource) ||
        resource.struct_version != FABRIC_ABI_VERSION_1 || !resource.path ||
        !resource.path[0]) {
      error = "malformed typed ROM resource";
      return FABRIC_INVALID_ARGUMENT;
    }
    if (resource.role != FABRIC_ROM_ROLE_PROGRAM &&
        resource.role != FABRIC_ROM_ROLE_SOUND &&
        resource.role != FABRIC_ROM_ROLE_OTHER) {
      error = "unknown typed ROM role";
      return FABRIC_INVALID_ARGUMENT;
    }
    if (!seen.insert({resource.role, resource.slot}).second) {
      error = "duplicate typed ROM role and slot";
      return FABRIC_INVALID_ARGUMENT;
    }
    if ((resource.role == FABRIC_ROM_ROLE_PROGRAM ||
         resource.role == FABRIC_ROM_ROLE_SOUND) && resource.slot >= 4) {
      error = "Amber program and sound ROM slots must be in the range 0..3";
      return FABRIC_INVALID_ARGUMENT;
    }
    if (resource.role == FABRIC_ROM_ROLE_PROGRAM)
      program.emplace_back(resource.slot, resource.path);
    else if (resource.role == FABRIC_ROM_ROLE_SOUND)
      sound.emplace_back(resource.slot, resource.path);
  }
  auto contiguous = [](std::vector<Rom> roms) {
    std::sort(roms.begin(), roms.end());
    for (size_t i = 0; i < roms.size(); ++i)
      if (roms[i].first != i)
        return false;
    return true;
  };
  if (!contiguous(program) || !contiguous(sound)) {
    error = "Amber program and sound ROM slots must be contiguous from slot zero";
    return FABRIC_INVALID_ARGUMENT;
  }
  return FABRIC_OK;
}

FabricResult validate_configuration(const FabricLaunchRequest &request,
                                    std::string &error) {
  if (!request.machine_configuration_size)
    return FABRIC_OK;
  if (!request.machine_configuration || request.machine_configuration_size !=
                                            sizeof(FabricAmberConfigurationV1)) {
    error = "malformed Amber backend configuration size";
    return FABRIC_INVALID_ARGUMENT;
  }
  const auto &c = *static_cast<const FabricAmberConfigurationV1 *>(
      request.machine_configuration);
  if (c.magic != FABRIC_AMBER_CONFIGURATION_MAGIC ||
      c.struct_size != sizeof(c) ||
      c.version != FABRIC_AMBER_CONFIGURATION_VERSION_1 ||
      (c.flags & ~UINT32_C(7))) {
    error = "malformed Amber backend configuration";
    return FABRIC_INVALID_ARGUMENT;
  }
  if ((c.flags & FABRIC_AMBER_CONFIGURE_REELS) &&
      (c.reels.struct_size != sizeof(c.reels) ||
       c.reels.version != FABRIC_AMBER_REEL_CONFIGURATION_VERSION_1 ||
       c.reels.reel_count > FABRIC_AMBER_MAX_REELS ||
       (c.reels.apply_mask & ~((UINT32_C(1) << FABRIC_AMBER_MAX_REELS) - 1)))) {
    error = "malformed Amber reel configuration";
    return FABRIC_INVALID_ARGUMENT;
  }
  if ((c.flags & FABRIC_AMBER_CONFIGURE_COINS) &&
      (c.coins.struct_size != sizeof(c.coins) ||
       c.coins.version != FABRIC_AMBER_COIN_CONFIGURATION_VERSION_1 ||
       (c.coins.channel_apply_mask &
        ~((UINT32_C(1) << FABRIC_AMBER_MAX_COIN_CHANNELS) - 1)) ||
       (c.coins.route_apply_mask &
        ~((UINT32_C(1) << FABRIC_AMBER_MAX_COIN_ROUTES) - 1)))) {
    error = "malformed Amber coin configuration";
    return FABRIC_INVALID_ARGUMENT;
  }
  if ((c.flags & FABRIC_AMBER_CONFIGURE_PERCENTAGE) &&
      c.percentage_switch > 15) {
    error = "Amber percentage switch must be in the range 0..15";
    return FABRIC_INVALID_ARGUMENT;
  }
  return FABRIC_OK;
}

class Provider final : public FabricBackendProvider {
public:
  bool supports(const std::string &kind,
                const std::string &) const noexcept override {
    return kind == "amber";
  }
  FabricResult create(const FabricLaunchRequest &request,
                      std::unique_ptr<FabricBackendInstance> &out,
                      std::string &error) noexcept override {
    try {
      if (!std::filesystem::path(request.backend_path).is_absolute()) {
        error = "Amber backend path must be absolute";
        return FABRIC_INVALID_ARGUMENT;
      }
      FabricResult result = validate_roms(request, error);
      if (result != FABRIC_OK)
        return result;
      result = validate_configuration(request, error);
      if (result != FABRIC_OK)
        return result;
      auto library = std::make_unique<AmberDynamicLibrary>();
      if (!library->open(request.backend_path, error))
        return FABRIC_NOT_FOUND;
      if (request.diagnostic_callback) {
        const char message[] = "[Fabric]\ncategory=amber.production\n"
                               "operation=AmberLibraryLoaded\nresult=success";
        try {
          request.diagnostic_callback(message, request.diagnostic_user_data);
        } catch (...) {
        }
      } else {
        amber_trace::Write("category=amber.production; operation=AmberLibraryLoaded; result=success");
      }
      amber_trace::Write("selected adapter: production");
      return CreateProductionAmberInstance(request, std::move(library), out,
                                           error);
    } catch (const std::exception &exception) {
      error = exception.what();
      return FABRIC_INTERNAL_ERROR;
    }
  }
};
} // namespace
std::unique_ptr<FabricBackendProvider> MakeAmberBackendProvider() {
  return std::make_unique<Provider>();
}
} // namespace fabric
