#ifndef FABRIC_FABRIC_AMBER_H
#define FABRIC_FABRIC_AMBER_H

#include "fabric.h"

#define FABRIC_AMBER_MAX_REELS 8u
#define FABRIC_AMBER_MAX_COIN_CHANNELS 6u
#define FABRIC_AMBER_MAX_COIN_ROUTES 8u
#define FABRIC_AMBER_SYSTEM6_REEL_CONFIGURATION_VERSION_1 1u
#define FABRIC_AMBER_SYSTEM6_COIN_CONFIGURATION_VERSION_2 2u

typedef struct FabricAmberSystem6ReelConfigV1 {
  uint32_t reel_index, enabled, steps, opto_start, opto_end, opto_invert;
} FabricAmberSystem6ReelConfigV1;
typedef struct FabricAmberSystem6ReelConfigurationV1 {
  uint32_t struct_size, version, reel_count, apply_mask;
  FabricAmberSystem6ReelConfigV1 reels[FABRIC_AMBER_MAX_REELS];
} FabricAmberSystem6ReelConfigurationV1;
typedef struct FabricAmberSystem6CoinChannelConfigV2 {
  uint32_t channel_index, enabled, value, lockout_invert, reserved;
} FabricAmberSystem6CoinChannelConfigV2;
typedef struct FabricAmberSystem6CoinRouteConfigV2 {
  uint32_t route_index, enabled, counter_in, counter_out, port_index, coin_code,
      level, full_level;
} FabricAmberSystem6CoinRouteConfigV2;
typedef struct FabricAmberSystem6CoinConfigurationV2 {
  uint32_t struct_size, version, channel_apply_mask, route_apply_mask;
  uint32_t coin_communication_style, coin_communication_invert,
      coin_pulse_cycles, coin_edc_enabled;
  FabricAmberSystem6CoinChannelConfigV2 channels[FABRIC_AMBER_MAX_COIN_CHANNELS];
  FabricAmberSystem6CoinRouteConfigV2 routes[FABRIC_AMBER_MAX_COIN_ROUTES];
} FabricAmberSystem6CoinConfigurationV2;

#define FABRIC_AMBER_SYSTEM6_CONFIGURATION_MAGIC UINT32_C(0x32424146)
#define FABRIC_AMBER_SYSTEM6_CONFIGURATION_VERSION_2 2u
typedef struct FabricAmberSystem6ConfigurationV2 {
  uint32_t magic, struct_size, version, flags;
  FabricAmberSystem6ReelConfigurationV1 reels;
  FabricAmberSystem6CoinConfigurationV2 coins;
  uint32_t percentage_switch, reserved[3];
} FabricAmberSystem6ConfigurationV2;
#define FABRIC_AMBER_SYSTEM6_CONFIGURE_REELS UINT32_C(1)
#define FABRIC_AMBER_SYSTEM6_CONFIGURE_COINS UINT32_C(2)
#define FABRIC_AMBER_SYSTEM6_CONFIGURE_PERCENTAGE UINT32_C(4)

/* Current Barcrest MPU5 configuration.  These declarations contain only
 * settings whose production export signatures are known. */
#define FABRIC_AMBER_MPU5_CONFIGURATION_MAGIC UINT32_C(0x354D4146)
#define FABRIC_AMBER_MPU5_CONFIGURATION_VERSION_1 1u
#define FABRIC_AMBER_MPU5_REEL_CONFIGURATION_VERSION_1 1u
#define FABRIC_AMBER_MPU5_COIN_CONFIGURATION_VERSION_1 1u
#define FABRIC_AMBER_MPU5_OPTIONS_VERSION_1 1u
#define FABRIC_AMBER_MPU5_REEL_JUMPER_CONTROLLER_COUNT 2u
#define FABRIC_AMBER_MPU5_DIP_COUNT 16u
#define FABRIC_AMBER_MPU5_CONFIGURE_REELS UINT32_C(1)
#define FABRIC_AMBER_MPU5_CONFIGURE_COINS UINT32_C(2)
#define FABRIC_AMBER_MPU5_CONFIGURE_OPTIONS UINT32_C(4)
#define FABRIC_AMBER_MPU5_OPTION_DIPS UINT32_C(1)
#define FABRIC_AMBER_MPU5_OPTION_STAKE UINT32_C(2)
#define FABRIC_AMBER_MPU5_OPTION_PRIZE UINT32_C(4)
#define FABRIC_AMBER_MPU5_OPTION_PERCENTAGE UINT32_C(8)
#define FABRIC_AMBER_MPU5_OPTION_CHARACTERISER_ADDRESS UINT32_C(16)
#define FABRIC_AMBER_MPU5_OPTION_PIC_MODE UINT32_C(32)
#define FABRIC_AMBER_MPU5_OPTION_SEC_FITTED UINT32_C(64)
#define FABRIC_AMBER_MPU5_OPTION_HOPPER_TYPE UINT32_C(128)
#define FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_0 UINT32_C(256)
#define FABRIC_AMBER_MPU5_OPTION_REEL_JUMPER_1 UINT32_C(512)

typedef struct FabricAmberMpu5ReelConfigV1 {
  uint32_t reel_index, steps, opto_start, opto_end, opto_invert;
} FabricAmberMpu5ReelConfigV1;
typedef struct FabricAmberMpu5ReelConfigurationV1 {
  uint32_t struct_size, version, reel_count, apply_mask;
  FabricAmberMpu5ReelConfigV1 reels[FABRIC_AMBER_MAX_REELS];
} FabricAmberMpu5ReelConfigurationV1;
typedef struct FabricAmberMpu5CoinChannelConfigV1 {
  uint32_t channel_index, enabled, value, lockout_invert, reserved;
} FabricAmberMpu5CoinChannelConfigV1;
typedef struct FabricAmberMpu5CoinConfigurationV1 {
  uint32_t struct_size, version, channel_count, apply_mask;
  uint32_t communication_style, communication_invert, pulse_cycles, edc_enabled;
  FabricAmberMpu5CoinChannelConfigV1 channels[FABRIC_AMBER_MAX_COIN_CHANNELS];
} FabricAmberMpu5CoinConfigurationV1;
typedef struct FabricAmberMpu5OptionsV1 {
  uint32_t struct_size, version, apply_mask;
  uint32_t dip_switch_bits, stake, prize, percentage, characteriser_address;
  uint32_t pic_mode, sec_fitted, hopper_type;
  uint32_t reel_jumper_profile_0, reel_jumper_profile_1;
  uint32_t reserved[2];
} FabricAmberMpu5OptionsV1;
typedef struct FabricAmberMpu5ConfigurationV1 {
  uint32_t magic, struct_size, version, flags;
  FabricAmberMpu5ReelConfigurationV1 reels;
  FabricAmberMpu5CoinConfigurationV1 coins;
  FabricAmberMpu5OptionsV1 options;
} FabricAmberMpu5ConfigurationV1;
#endif
