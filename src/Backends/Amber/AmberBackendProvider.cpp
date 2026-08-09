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
        resource.struct_version != FABRIC_ABI_VERSION_CURRENT ||
        !resource.path || !resource.path[0]) {
      error = "malformed typed ROM resource";
      return FABRIC_INVALID_ARGUMENT;
    }
    if (resource.reserved) {
      error = "ROM resource reserved field must be zero";
      return FABRIC_INVALID_ARGUMENT;
    }
    if (std::string(request.machine_identifier) == "barcrest-mpu3" &&
        (resource.role == FABRIC_ROM_ROLE_SOUND || resource.load_address > INT32_MAX)) {
      error = "MPU3 accepts only directly-addressed program ROMs";
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
  if (!request.machine_configuration_size &&
      std::string(request.machine_identifier) == "barcrest-mpu3") {
    error = "Amber machine 'barcrest-mpu3' requires FabricAmberMpu3Config";
    return FABRIC_INVALID_ARGUMENT;
  }
  if (!request.machine_configuration_size)
    return FABRIC_OK;
  const std::string machine = request.machine_identifier;
  auto malformed = [&](const std::string &detail) {
    error = "Amber machine '" + machine + "' configuration " + detail +
            "; provider DLL='" + request.backend_path + "'";
    return FABRIC_INVALID_ARGUMENT;
  };
  if (!request.machine_configuration)
    return malformed("is null");
  if (machine == "barcrest-mpu3") {
    if (request.machine_configuration_size != sizeof(FabricAmberMpu3Config))
      return malformed("expected FabricAmberMpu3Config (" +
          std::to_string(sizeof(FabricAmberMpu3Config)) + " bytes), received " +
          std::to_string(request.machine_configuration_size) + " bytes");
    const auto &c = *static_cast<const FabricAmberMpu3Config *>(request.machine_configuration);
    if (c.magic != FABRIC_AMBER_MPU3_CONFIGURATION_MAGIC || c.struct_size != sizeof(c) ||
        c.version != FABRIC_AMBER_MPU3_CONFIGURATION_VERSION_1 ||
        c.reel_count != FABRIC_AMBER_MPU3_REEL_COUNT)
      return malformed("is not a valid FabricAmberMpu3Config");
    for (uint32_t i = 0; i < FABRIC_AMBER_MPU3_REEL_COUNT; ++i)
      if (!c.reels[i].steps || c.reels[i].opto_invert > 1)
        return malformed("has an invalid MPU3 reel entry");
    for (uint32_t i = 0; i < FABRIC_AMBER_MPU3_DIP_COUNT; ++i)
      if (c.dips[i] > 1) return malformed("has an invalid MPU3 DIP value");
    return FABRIC_OK;
  }
  if (machine == "jpm-system6") {
    if (request.machine_configuration_size !=
        sizeof(FabricAmberSystem6ConfigurationV2))
      return malformed("expected FabricAmberSystem6ConfigurationV2 (" +
                       std::to_string(sizeof(FabricAmberSystem6ConfigurationV2)) +
                       " bytes), received " +
                       std::to_string(request.machine_configuration_size) + " bytes");
    const auto &c = *static_cast<const FabricAmberSystem6ConfigurationV2 *>(
        request.machine_configuration);
    if (c.magic != FABRIC_AMBER_SYSTEM6_CONFIGURATION_MAGIC ||
        c.struct_size != sizeof(c) ||
        c.version != FABRIC_AMBER_SYSTEM6_CONFIGURATION_VERSION_2 ||
        (c.flags & ~UINT32_C(7)) || c.reserved[0] || c.reserved[1] ||
        c.reserved[2])
      return malformed("is not a valid FabricAmberSystem6ConfigurationV2");
    if ((c.flags & FABRIC_AMBER_SYSTEM6_CONFIGURE_REELS) &&
        (c.reels.struct_size != sizeof(c.reels) ||
         c.reels.version != FABRIC_AMBER_SYSTEM6_REEL_CONFIGURATION_VERSION_1 ||
         c.reels.reel_count > FABRIC_AMBER_MAX_REELS ||
         (c.reels.apply_mask & ~UINT32_C(0xff))))
      return malformed("has an invalid System 6 reel section");
    if ((c.flags & FABRIC_AMBER_SYSTEM6_CONFIGURE_COINS) &&
        (c.coins.struct_size != sizeof(c.coins) ||
         c.coins.version != FABRIC_AMBER_SYSTEM6_COIN_CONFIGURATION_VERSION_2 ||
         (c.coins.channel_apply_mask & ~UINT32_C(0x3f)) ||
         (c.coins.route_apply_mask & ~UINT32_C(0xff)) ||
         c.coins.coin_communication_style != 0 ||
         c.coins.coin_communication_invert > 1 ||
         !c.coins.coin_pulse_cycles || c.coins.coin_edc_enabled > 1))
      return malformed("has an invalid System 6 coin section");
    for (uint32_t i = 0; i < FABRIC_AMBER_MAX_COIN_CHANNELS; ++i)
      if ((c.flags & FABRIC_AMBER_SYSTEM6_CONFIGURE_COINS) &&
          (c.coins.channel_apply_mask & (UINT32_C(1) << i))) {
        const auto &v = c.coins.channels[i];
        if (v.channel_index != i || v.enabled > 1 || v.value > 12 ||
            v.lockout_invert > 1 || v.reserved)
          return malformed("has an invalid System 6 coin channel");
      }
    if ((c.flags & FABRIC_AMBER_SYSTEM6_CONFIGURE_PERCENTAGE) &&
        c.percentage_switch > 15)
      return malformed("has a System 6 percentage outside 0..15");
    return FABRIC_OK;
  }
  if (machine == "maygay-epoch") {
    if (request.machine_configuration_size != sizeof(FabricAmberEpochConfigurationV1))
      return malformed("expected FabricAmberEpochConfigurationV1 (" +
                       std::to_string(sizeof(FabricAmberEpochConfigurationV1)) +
                       " bytes), received " + std::to_string(request.machine_configuration_size) + " bytes");
    const auto &c = *static_cast<const FabricAmberEpochConfigurationV1 *>(request.machine_configuration);
    constexpr uint32_t flags = FABRIC_AMBER_EPOCH_CONFIGURE_REELS |
        FABRIC_AMBER_EPOCH_CONFIGURE_COINS | FABRIC_AMBER_EPOCH_CONFIGURE_OPTIONS |
        FABRIC_AMBER_EPOCH_CONFIGURE_REEL_EXT;
    if (c.magic != FABRIC_AMBER_EPOCH_CONFIGURATION_MAGIC || c.struct_size != sizeof(c) ||
        c.version != FABRIC_AMBER_EPOCH_CONFIGURATION_VERSION_1 || (c.flags & ~flags) ||
        c.flash_rom_mode > 1 || c.reel_count > 8 || c.coin_channel_count > 6 ||
        (c.reel_apply_mask & ~UINT32_C(0xff)) || (c.coin_apply_mask & ~UINT32_C(0x3f)) ||
        c.communication_invert > 1 || c.edc_enabled > 1 || c.reel_ext > 255 ||
        (c.options_apply_mask & ~UINT32_C(15)) || (c.dip_switch_bits & ~UINT32_C(0xffff)) ||
        c.stake > 255 || c.prize > 255 || c.percentage > 255 ||
        c.reserved[0] || c.reserved[1] || c.reserved[2] || c.reserved[3])
      return malformed("is not a valid FabricAmberEpochConfigurationV1");
    for (uint32_t i = 0; i < 8; ++i) if (c.reel_apply_mask & (UINT32_C(1) << i)) {
      const auto &v = c.reels[i];
      if (i >= c.reel_count || v.reel_index != i || !v.steps || v.steps > 255 ||
          v.opto_start > 255 || v.opto_end > 255 || v.opto_invert > 1)
        return malformed("has an invalid Epoch reel entry");
    }
    for (uint32_t i = 0; i < 6; ++i) if (c.coin_apply_mask & (UINT32_C(1) << i)) {
      const auto &v = c.coins[i];
      if (i >= c.coin_channel_count || v.channel_index != i || v.enabled > 1 ||
          v.value > 12 || v.lockout_value > 12 || v.lockout_invert > 1)
        return malformed("has an invalid Epoch coin channel");
    }
    return FABRIC_OK;
  }
  if (request.machine_configuration_size != sizeof(FabricAmberMpu5ConfigurationV1))
    return malformed("expected FabricAmberMpu5ConfigurationV1 (" +
                     std::to_string(sizeof(FabricAmberMpu5ConfigurationV1)) +
                     " bytes), received " +
                     std::to_string(request.machine_configuration_size) + " bytes");
  const auto &c = *static_cast<const FabricAmberMpu5ConfigurationV1 *>(
      request.machine_configuration);
  if (c.magic != FABRIC_AMBER_MPU5_CONFIGURATION_MAGIC ||
      c.struct_size != sizeof(c) ||
      c.version != FABRIC_AMBER_MPU5_CONFIGURATION_VERSION_1 ||
      (c.flags & ~(FABRIC_AMBER_MPU5_CONFIGURE_REELS |
                   FABRIC_AMBER_MPU5_CONFIGURE_COINS |
                   FABRIC_AMBER_MPU5_CONFIGURE_OPTIONS)))
    return malformed("is not a valid FabricAmberMpu5ConfigurationV1");
  if (c.reels.struct_size != sizeof(c.reels) ||
      c.reels.version != FABRIC_AMBER_MPU5_REEL_CONFIGURATION_VERSION_1 ||
      c.coins.struct_size != sizeof(c.coins) ||
      c.coins.version != FABRIC_AMBER_MPU5_COIN_CONFIGURATION_VERSION_1 ||
      c.options.struct_size != sizeof(c.options) ||
      c.options.version != FABRIC_AMBER_MPU5_OPTIONS_VERSION_1)
    return malformed("has an invalid MPU5 nested section identity");
  if ((c.flags & FABRIC_AMBER_MPU5_CONFIGURE_REELS) &&
      (c.reels.reel_count > FABRIC_AMBER_MAX_REELS ||
       (c.reels.apply_mask & ~UINT32_C(0xff)) ||
       (c.reels.apply_mask &
        ~((UINT32_C(1) << c.reels.reel_count) - UINT32_C(1)))))
    return malformed("has an invalid MPU5 reel section");
  for (uint32_t i = 0; i < FABRIC_AMBER_MAX_REELS; ++i)
    if ((c.flags & FABRIC_AMBER_MPU5_CONFIGURE_REELS) &&
        (c.reels.apply_mask & (UINT32_C(1) << i))) {
      const auto &v = c.reels.reels[i];
      if (v.reel_index != i || !v.steps || v.steps > 255 ||
          v.opto_start > 255 || v.opto_end > 255 || v.opto_invert > 1)
        return malformed("has an invalid MPU5 reel entry");
    }
  if ((c.flags & FABRIC_AMBER_MPU5_CONFIGURE_COINS) &&
      (c.coins.channel_count > FABRIC_AMBER_MAX_COIN_CHANNELS ||
       (c.coins.apply_mask & ~UINT32_C(0x3f)) ||
       (c.coins.apply_mask &
        ~((UINT32_C(1) << c.coins.channel_count) - UINT32_C(1))) ||
       c.coins.communication_style > 3 || c.coins.communication_invert > 1 ||
       !c.coins.pulse_cycles || c.coins.edc_enabled > 1))
    return malformed("has an invalid MPU5 coin section");
  for (uint32_t i = 0; i < FABRIC_AMBER_MAX_COIN_CHANNELS; ++i)
    if ((c.flags & FABRIC_AMBER_MPU5_CONFIGURE_COINS) &&
        (c.coins.apply_mask & (UINT32_C(1) << i))) {
      const auto &v = c.coins.channels[i];
      if (v.channel_index != i || v.enabled > 1 || v.value > 255 ||
          v.lockout_invert > 1 || v.reserved)
        return malformed("has an invalid MPU5 coin channel");
    }
  constexpr uint32_t option_mask =
      FABRIC_AMBER_MPU5_OPTION_DIPS | FABRIC_AMBER_MPU5_OPTION_STAKE |
      FABRIC_AMBER_MPU5_OPTION_PRIZE | FABRIC_AMBER_MPU5_OPTION_PERCENTAGE |
      FABRIC_AMBER_MPU5_OPTION_CHARACTERISER_ADDRESS |
      FABRIC_AMBER_MPU5_OPTION_PIC_MODE | FABRIC_AMBER_MPU5_OPTION_SEC_FITTED |
      FABRIC_AMBER_MPU5_OPTION_HOPPER_TYPE |
      FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_0 |
      FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_1;
  const auto &o = c.options;
  if ((c.flags & FABRIC_AMBER_MPU5_CONFIGURE_OPTIONS) &&
      ((o.apply_mask & ~option_mask) || (o.dip_switch_bits & ~UINT32_C(0xffff)) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_STAKE) && o.stake > 255) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_PRIZE) && o.prize > 255) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_PERCENTAGE) && o.percentage > 15) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_PIC_MODE) &&
        (o.pic_mode < 1 || o.pic_mode > 3)) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_SEC_FITTED) && o.sec_fitted > 1) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_HOPPER_TYPE) && o.hopper_type > 3) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_0) &&
        o.reel_jumper_profile_0 > 2) ||
       ((o.apply_mask & FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_1) &&
        o.reel_jumper_profile_1 > 2)))
    return malformed("has an invalid MPU5 options section");
  if (o.reserved[0] || o.reserved[1])
    return malformed("has nonzero reserved fields in FabricAmberMpu5OptionsV1");
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
      const std::string machine = request.machine_identifier;
      if (machine != "jpm-system6" && machine != "barcrest-mpu5" &&
          machine != "maygay-epoch" && machine != "barcrest-mpu3") {
        error = "Amber backend 'amber' does not support machine identifier '" +
                machine + "'; provider DLL='" + request.backend_path + "'";
        return FABRIC_NOT_SUPPORTED;
      }
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
