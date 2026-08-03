#ifndef FABRIC_FABRIC_AMBER_H
#define FABRIC_FABRIC_AMBER_H

#include "fabric.h"

#define FABRIC_AMBER_MAX_REELS 8u
#define FABRIC_AMBER_MAX_COIN_CHANNELS 6u
#define FABRIC_AMBER_MAX_COIN_ROUTES 8u
#define FABRIC_AMBER_REEL_CONFIGURATION_VERSION_1 1u
#define FABRIC_AMBER_COIN_CONFIGURATION_VERSION_2 2u

typedef struct FabricAmberReelConfigV1 {
  uint32_t reel_index, enabled, steps, opto_start, opto_end, opto_invert;
} FabricAmberReelConfigV1;
typedef struct FabricAmberReelConfigurationV1 {
  uint32_t struct_size, version, reel_count, apply_mask;
  FabricAmberReelConfigV1 reels[FABRIC_AMBER_MAX_REELS];
} FabricAmberReelConfigurationV1;
typedef struct FabricAmberCoinChannelConfigV2 {
  uint32_t channel_index, enabled, value, lockout_invert, reserved;
} FabricAmberCoinChannelConfigV2;
typedef struct FabricAmberCoinRouteConfigV2 {
  uint32_t route_index, enabled, counter_in, counter_out, port_index, coin_code,
      level, full_level;
} FabricAmberCoinRouteConfigV2;
typedef struct FabricAmberCoinConfigurationV2 {
  uint32_t struct_size, version, channel_apply_mask, route_apply_mask;
  uint32_t coin_communication_style, coin_communication_invert,
      coin_pulse_cycles, coin_edc_enabled;
  FabricAmberCoinChannelConfigV2 channels[FABRIC_AMBER_MAX_COIN_CHANNELS];
  FabricAmberCoinRouteConfigV2 routes[FABRIC_AMBER_MAX_COIN_ROUTES];
} FabricAmberCoinConfigurationV2;

#define FABRIC_AMBER_CONFIGURATION_MAGIC UINT32_C(0x32424146)
#define FABRIC_AMBER_CONFIGURATION_VERSION_2 2u
typedef struct FabricAmberConfigurationV2 {
  uint32_t magic, struct_size, version, flags;
  FabricAmberReelConfigurationV1 reels;
  FabricAmberCoinConfigurationV2 coins;
  uint32_t percentage_switch, reserved[3];
} FabricAmberConfigurationV2;
#define FABRIC_AMBER_CONFIGURE_REELS UINT32_C(1)
#define FABRIC_AMBER_CONFIGURE_COINS UINT32_C(2)
#define FABRIC_AMBER_CONFIGURE_PERCENTAGE UINT32_C(4)
#endif
