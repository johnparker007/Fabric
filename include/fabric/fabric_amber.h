#ifndef FABRIC_FABRIC_AMBER_H
#define FABRIC_FABRIC_AMBER_H

#include "fabric.h"

#define FABRIC_AMBER_MAX_REELS 8u
#define FABRIC_AMBER_MAX_COIN_CHANNELS 6u
#define FABRIC_AMBER_MAX_COIN_ROUTES 8u
#define FABRIC_AMBER_REEL_CONFIGURATION_VERSION_1 1u
#define FABRIC_AMBER_COIN_CONFIGURATION_VERSION_1 1u
#define FABRIC_AMBER_COIN_CONFIG_APPLY_LOCKOUT_PORT UINT32_C(1)

typedef struct FabricAmberReelConfigV1 {
  uint32_t reel_index, enabled, steps, opto_start, opto_end, opto_invert;
} FabricAmberReelConfigV1;
typedef struct FabricAmberReelConfigurationV1 {
  uint32_t struct_size, version, reel_count, apply_mask;
  FabricAmberReelConfigV1 reels[FABRIC_AMBER_MAX_REELS];
} FabricAmberReelConfigurationV1;
typedef struct FabricAmberCoinChannelConfigV1 {
  uint32_t channel_index, enabled, value, lockout_invert, reserved;
} FabricAmberCoinChannelConfigV1;
typedef struct FabricAmberCoinRouteConfigV1 {
  uint32_t route_index, enabled, counter_in, counter_out, port_index, coin_code,
      level, full_level;
} FabricAmberCoinRouteConfigV1;
typedef struct FabricAmberCoinConfigurationV1 {
  uint32_t struct_size, version, channel_apply_mask, route_apply_mask;
  FabricAmberCoinChannelConfigV1 channels[FABRIC_AMBER_MAX_COIN_CHANNELS];
  FabricAmberCoinRouteConfigV1 routes[FABRIC_AMBER_MAX_COIN_ROUTES];
  uint32_t lockout_port_base, lockout_port_value, configuration_flags, reserved;
} FabricAmberCoinConfigurationV1;

#define FABRIC_AMBER_CONFIGURATION_MAGIC UINT32_C(0x32424146)
#define FABRIC_AMBER_CONFIGURATION_VERSION_1 1u
typedef struct FabricAmberConfigurationV1 {
  uint32_t magic, struct_size, version, flags;
  FabricAmberReelConfigurationV1 reels;
  FabricAmberCoinConfigurationV1 coins;
  uint32_t percentage_switch, reserved[3];
} FabricAmberConfigurationV1;
#define FABRIC_AMBER_CONFIGURE_REELS UINT32_C(1)
#define FABRIC_AMBER_CONFIGURE_COINS UINT32_C(2)
#define FABRIC_AMBER_CONFIGURE_PERCENTAGE UINT32_C(4)
#endif
