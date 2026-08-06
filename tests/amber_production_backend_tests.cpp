#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << "check failed line " << __LINE__ << ": " #x "\n";           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static FabricLaunchRequest request(const char *path) {
  FabricLaunchRequest r{};
  r.struct_size = sizeof(r);
  r.struct_version = FABRIC_ABI_VERSION_CURRENT;
  std::strcpy(r.backend_kind, "amber");
  std::strcpy(r.machine_identifier, "jpm-system6");
  std::strncpy(r.backend_path, path, sizeof(r.backend_path) - 1);
  return r;
}
static std::string error(FabricRuntime *r) {
  char b[512]{};
  uint32_t n = 0;
  FabricRuntimeGetLastError(r, b, sizeof(b), &n);
  return b;
}
static std::string session_error(FabricMachineSession *s) {
  char b[512]{};
  uint32_t n = 0;
  FabricSessionGetLastError(s, b, sizeof(b), &n);
  return b;
}
static void FABRIC_CALL diagnostic(const char *message, void *context) {
  static_cast<std::vector<std::string> *>(context)->emplace_back(message);
}

static FabricAmberMpu5ConfigurationV1 mpu5_configuration() {
  FabricAmberMpu5ConfigurationV1 c{};
  c.magic = FABRIC_AMBER_MPU5_CONFIGURATION_MAGIC;
  c.struct_size = sizeof(c);
  c.version = FABRIC_AMBER_MPU5_CONFIGURATION_VERSION_1;
  c.flags = FABRIC_AMBER_MPU5_CONFIGURE_REELS |
            FABRIC_AMBER_MPU5_CONFIGURE_COINS |
            FABRIC_AMBER_MPU5_CONFIGURE_PERCENTAGE;
  c.reels.struct_size = sizeof(c.reels);
  c.reels.version = FABRIC_AMBER_MPU5_REEL_CONFIGURATION_VERSION_1;
  c.reels.reel_count = 2;
  c.reels.apply_mask = UINT32_C(1) << 1;
  c.reels.reels[1].reel_index = 1;
  c.reels.reels[1].steps = 96;
  c.reels.reels[1].opto_start = 7;
  c.reels.reels[1].opto_end = 11;
  c.reels.reels[1].opto_invert = 1;
  c.coins.struct_size = sizeof(c.coins);
  c.coins.version = FABRIC_AMBER_MPU5_COIN_CONFIGURATION_VERSION_1;
  c.coins.channel_count = 3;
  c.coins.apply_mask = UINT32_C(1) << 2;
  c.coins.channels[2].channel_index = 2;
  c.coins.channels[2].enabled = 1;
  c.coins.channels[2].value = 5;
  c.coins.channels[2].lockout_invert = 1;
  c.percentage = 9;
  return c;
}

int main() {
  FabricRuntime *runtime = nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT, &runtime) == FABRIC_OK);
  auto bad = request(FAKE_PRODUCTION_AMBER_MISSING_REQUIRED_PATH);
  FabricMachineSession *session = nullptr;
  CHECK(FabricCreateSession(runtime, &bad, &session) == FABRIC_NOT_SUPPORTED);
  CHECK(error(runtime).find("required export 'Initialise'") !=
        std::string::npos);
  auto missing_coin = request(FAKE_PRODUCTION_AMBER_MISSING_COIN_IN_PATH);
  CHECK(FabricCreateSession(runtime, &missing_coin, &session) == FABRIC_NOT_SUPPORTED);
  CHECK(error(runtime).find("required export 'CoinIn'") != std::string::npos);
  auto missing_mechanism = request(FAKE_PRODUCTION_AMBER_MISSING_MECHANISM_PATH);
  CHECK(FabricCreateSession(runtime, &missing_mechanism, &session) == FABRIC_NOT_SUPPORTED);
  CHECK(error(runtime).find("required export 'SetCommStyle'") != std::string::npos);
  const char *program[] = {"program-even.rom", "program-odd.rom"};
  FabricRomResource roms[3]{};
  for (auto &r : roms) {
    r.struct_size = sizeof(r);
    r.struct_version = FABRIC_ABI_VERSION_CURRENT;
  }
  roms[0].role = FABRIC_ROM_ROLE_PROGRAM;
  roms[0].slot = 1;
  roms[0].path = program[1];
  roms[1].role = FABRIC_ROM_ROLE_SOUND;
  roms[1].slot = 0;
  roms[1].path = "sound.rom";
  roms[2].role = FABRIC_ROM_ROLE_PROGRAM;
  roms[2].slot = 0;
  roms[2].path = program[0];
  FabricAmberSystem6ConfigurationV2 c{};
  c.magic = FABRIC_AMBER_SYSTEM6_CONFIGURATION_MAGIC;
  c.struct_size = sizeof(c);
  c.version = FABRIC_AMBER_SYSTEM6_CONFIGURATION_VERSION_2;
  c.flags = FABRIC_AMBER_SYSTEM6_CONFIGURE_REELS | FABRIC_AMBER_SYSTEM6_CONFIGURE_COINS |
            FABRIC_AMBER_SYSTEM6_CONFIGURE_PERCENTAGE;
  c.reels.struct_size = sizeof(c.reels);
  c.reels.version = FABRIC_AMBER_SYSTEM6_REEL_CONFIGURATION_VERSION_1;
  c.reels.reel_count = 1;
  c.reels.apply_mask = 1;
  c.reels.reels[0].steps = 9;
  c.reels.reels[0].enabled = 1;
  c.coins.struct_size = sizeof(c.coins);
  c.coins.version = FABRIC_AMBER_SYSTEM6_COIN_CONFIGURATION_VERSION_2;
  c.coins.channel_apply_mask = 1;
  c.coins.coin_communication_style = 0;
  c.coins.coin_communication_invert = 0;
  c.coins.coin_pulse_cycles = 800000;
  c.coins.coin_edc_enabled = 0;
  c.coins.channels[0].channel_index = 0;
  c.coins.channels[0].value = 4;
  c.coins.channels[0].enabled = 1;
  c.percentage_switch = 7;
  auto obsolete_configuration = c;
  obsolete_configuration.version = 1;
  auto obsolete_request = request(FAKE_PRODUCTION_AMBER_PATH);
  obsolete_request.machine_configuration = &obsolete_configuration;
  obsolete_request.machine_configuration_size = sizeof(obsolete_configuration);
  CHECK(FabricCreateSession(runtime, &obsolete_request, &session) ==
        FABRIC_INVALID_ARGUMENT);
  auto good = request(FAKE_PRODUCTION_AMBER_PATH);
  good.rom_resources = roms;
  good.rom_resource_count = 3;
  good.machine_configuration = &c;
  good.machine_configuration_size = sizeof(c);
  std::vector<std::string> diagnostics;
  good.diagnostic_callback = diagnostic;
  good.diagnostic_user_data = &diagnostics;
  CHECK(FabricCreateSession(runtime, &good, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_OK);
  CHECK(FabricSessionReset(session) == FABRIC_OK);
  FabricCapabilities capabilities{sizeof(FabricCapabilities), FABRIC_ABI_VERSION_CURRENT, 0, {0}};
  CHECK(FabricSessionGetCapabilities(session, &capabilities) == FABRIC_OK);
  CHECK((capabilities.flags & FABRIC_CAPABILITY_DIGITAL_INPUT) != 0);
  CHECK((capabilities.flags & FABRIC_CAPABILITY_COIN_INPUT) != 0);
  FabricAudioFormat format{};
  format.struct_size = sizeof(format);
  format.struct_version = FABRIC_ABI_VERSION_CURRENT;
  CHECK(FabricSessionGetAudioFormat(session, &format) == FABRIC_OK);
  CHECK(format.sample_rate == 44100 && format.channel_count == 2);
  CHECK(FabricSessionAdvance(session, 500000) == FABRIC_OK);
  FabricInput input{};
  input.struct_size = sizeof(input);
  input.struct_version = FABRIC_ABI_VERSION_CURRENT;
  input.numerical_index = 7;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  FabricLamp lamps[512]{};
  FabricReel reels[8]{};
  FabricCharacterDisplay chars[1]{};
  FabricSegmentDisplay segments[16]{};
  FabricMachineSnapshot snap{};
  snap.struct_size = sizeof(snap);
  snap.struct_version = FABRIC_ABI_VERSION_CURRENT;
  snap.lamps = lamps;
  snap.lamp_capacity = 512;
  snap.reels = reels;
  snap.reel_capacity = 8;
  snap.character_displays = chars;
  snap.character_display_capacity = 1;
  snap.segment_displays = segments;
  snap.segment_display_capacity = 16;
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(lamps[0].logical_state == 1 && lamps[1].logical_state == 1 &&
        lamps[1].brightness == 4.0f);
  CHECK(reels[0].position == 9); /* A partial tick performs no native Run. */
  CHECK(chars[0].characters[0] == 0x1234 && chars[0].attributes[0] == 1 &&
        chars[0].brightness == 0.5f);
  CHECK(segments[0].segment_masks[0] == 0x5a);
  CHECK(lamps[3].brightness == 3.0f && lamps[4].brightness >= 1.0f);
  CHECK(lamps[8].brightness == 8.0f && lamps[9].brightness == 800000.0f);
  CHECK(lamps[10].brightness == 0.0f && lamps[11].brightness == 0.0f);

  /* Coin actions carry an explicit channel and denomination. A held action is
   * edge-triggered; release only rearms it and never calls Amber. */
  const float switch_calls_before_coins = lamps[7].brightness;
  input.kind = FABRIC_INPUT_COIN;
  input.coin_channel = 0;
  input.coin_value = 0;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(lamps[5].brightness == 1.0f && lamps[6].brightness == 0.0f);
  CHECK(lamps[7].brightness == switch_calls_before_coins);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.coin_channel = 5;
  input.coin_value = 12;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.coin_channel = 6;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_INVALID_ARGUMENT);
  input.coin_channel = 5;
  input.coin_value = 13;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_INVALID_ARGUMENT);
  input.coin_value = 11;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_INPUT_REJECTED);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.kind = FABRIC_INPUT_DIGITAL;
  input.coin_channel = input.coin_value = 0;

  /* Exercise alpha brightness through the complete fake Amber -> adapter ->
   * public snapshot path, while checking the other display transports stay
   * unchanged. */
  input.numerical_index = 240;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(chars[0].brightness == 0.0f && chars[0].characters[0] == 0x1234 &&
        chars[0].attributes[0] == 1);
  CHECK(segments[0].segment_masks[0] == 0x5a &&
        lamps[0].brightness == 0.75f);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.numerical_index = 241;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(chars[0].brightness == 1.0f && chars[0].characters[0] == 0x1234 &&
        chars[0].attributes[0] == 1);
  CHECK(segments[0].segment_masks[0] == 0x5a &&
        lamps[0].brightness == 0.75f);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.numerical_index = 249;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_BACKEND_ERROR);
  CHECK(session_error(session).find(
            "alpha-display brightness is non-finite; index=0") !=
        std::string::npos);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.numerical_index = 248;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_BACKEND_ERROR);
  CHECK(session_error(session).find(
            "alpha-display brightness is outside 0.0..1.0; index=0") !=
        std::string::npos);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.numerical_index = 7;
  input.active = 1;
  CHECK(FabricSessionAdvance(session, 500000) == FABRIC_OK);
  CHECK(FabricSessionAdvance(session, 2000000) == FABRIC_OK);
  CHECK(FabricSessionAdvance(session, 5000000) == FABRIC_OK);
  CHECK(FabricSessionAdvance(session, 30000000) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(reels[0].position == 72009); /* 1 + 2 + 3 + 3 fixed ticks. */
  CHECK(FabricSessionReset(session) == FABRIC_OK);
  CHECK(FabricSessionReset(session) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(lamps[1].logical_state == 1 && lamps[1].brightness == 4.0f);
  CHECK(lamps[3].brightness == 3.0f && lamps[4].brightness >= 3.0f);
  int16_t audio[8]{};
  uint32_t written = 0;
  CHECK(FabricSessionAdvance(session, 1000000) == FABRIC_OK);
  CHECK(FabricSessionReadAudio(session, audio, 4, &written) == FABRIC_OK);
  CHECK(written == 4 && audio[0] == 100 && audio[7] == 107);
  /* Zero is the fake's normal Run return. It must not cause retry or failure;
   * a long delay is clamped to exactly three fixed native calls. */
  CHECK(FabricSessionAdvance(session, (static_cast<uint64_t>(INT32_MAX) + 5u) *
                                          125u) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(lamps[2].brightness == 13.0f);
  uint32_t run_diagnostics = 0;
  for (const auto &message : diagnostics) {
    if (message.find("operation=AmberRun") != std::string::npos) {
      ++run_diagnostics;
      CHECK(message.find("requested_cycles=8000") != std::string::npos);
    }
  }
  CHECK(run_diagnostics == 13);
  bool selected = false, loaded = false;
  for (const auto &message : diagnostics) {
    selected |= message.find("operation=AdapterSelected") != std::string::npos;
    loaded |= message.find("operation=AmberLibraryLoaded") != std::string::npos;
  }
  CHECK(selected && loaded);
  CHECK(FabricSessionReadAudio(session, audio, 4, &written) == FABRIC_OK);
  input.numerical_index = 250;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_BACKEND_ERROR);
  CHECK(session_error(session).find("GetOutputSnapshot failed") !=
        std::string::npos);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionShutdown(session) == FABRIC_OK);
  FabricDestroySession(session);
  /* A second load proves the first module was unloadable and its singleton
   * reset. */
  session = nullptr;
  CHECK(FabricCreateSession(runtime, &good, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_OK);
  CHECK(FabricSessionShutdown(session) == FABRIC_OK);
  FabricDestroySession(session);
  auto missing = request(FAKE_PRODUCTION_AMBER_MISSING_CONFIG_PATH);
  missing.rom_resources = roms;
  missing.rom_resource_count = 3;
  missing.machine_configuration = &c;
  missing.machine_configuration_size = sizeof(c);
  session = nullptr;
  CHECK(FabricCreateSession(runtime, &missing, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_NOT_SUPPORTED);
  CHECK(session_error(session).find("missing reel export") !=
        std::string::npos);
  FabricDestroySession(session);
  auto wrong_for_system6 = request(FAKE_PRODUCTION_AMBER_PATH);
  auto mpu_config = mpu5_configuration();
  wrong_for_system6.machine_configuration = &mpu_config;
  wrong_for_system6.machine_configuration_size = sizeof(mpu_config);
  CHECK(FabricCreateSession(runtime, &wrong_for_system6, &session) ==
        FABRIC_INVALID_ARGUMENT);
  CHECK(error(runtime).find("FabricAmberSystem6ConfigurationV2") !=
        std::string::npos);

  auto mpu5 = request(FAKE_PRODUCTION_AMBER_MPU5_PATH);
  std::strcpy(mpu5.machine_identifier, "barcrest-mpu5");
  mpu5.rom_resources = roms;
  mpu5.rom_resource_count = 3;
  auto wrong_for_mpu5 = mpu5;
  wrong_for_mpu5.machine_configuration = &c;
  wrong_for_mpu5.machine_configuration_size = sizeof(c);
  CHECK(FabricCreateSession(runtime, &wrong_for_mpu5, &session) ==
        FABRIC_INVALID_ARGUMENT);
  CHECK(error(runtime).find("FabricAmberMpu5ConfigurationV1") !=
        std::string::npos);
  auto invalid_mpu = [&](FabricAmberMpu5ConfigurationV1 invalid,
                         uint32_t supplied_size =
                             sizeof(FabricAmberMpu5ConfigurationV1)) {
    auto r = mpu5;
    r.machine_configuration = &invalid;
    r.machine_configuration_size = supplied_size;
    FabricMachineSession *invalid_session = nullptr;
    const FabricResult result =
        FabricCreateSession(runtime, &r, &invalid_session);
    if (invalid_session)
      FabricDestroySession(invalid_session);
    return result;
  };
  CHECK(invalid_mpu(mpu_config, sizeof(mpu_config) - 4) ==
        FABRIC_INVALID_ARGUMENT);
  auto invalid = mpu_config;
  invalid.magic ^= 1;
  CHECK(invalid_mpu(invalid) == FABRIC_INVALID_ARGUMENT);
  invalid = mpu_config; ++invalid.version;
  CHECK(invalid_mpu(invalid) == FABRIC_INVALID_ARGUMENT);
  invalid = mpu_config; invalid.flags |= UINT32_C(0x80000000);
  CHECK(invalid_mpu(invalid) == FABRIC_INVALID_ARGUMENT);
  invalid = mpu_config; invalid.reserved[4] = 1;
  CHECK(invalid_mpu(invalid) == FABRIC_INVALID_ARGUMENT);
  invalid = mpu_config; invalid.reels.reels[1].reel_index = 7;
  CHECK(invalid_mpu(invalid) == FABRIC_INVALID_ARGUMENT);
  invalid = mpu_config; invalid.coins.channels[2].channel_index = 5;
  CHECK(invalid_mpu(invalid) == FABRIC_INVALID_ARGUMENT);

  /* No MPU5 configuration remains a valid launch. */
  session = nullptr;
  CHECK(FabricCreateSession(runtime, &mpu5, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_OK);
  CHECK(FabricSessionAdvance(session, 1000000) == FABRIC_OK);
  std::vector<FabricLamp> mpu_lamps(320);
  std::vector<FabricReel> mpu_reels(8);
  std::vector<FabricCharacterDisplay> mpu_alpha(2);
  std::vector<FabricSegmentDisplay> mpu_segments(40);
  FabricMachineSnapshot mpu_snap{};
  mpu_snap.struct_size = sizeof(mpu_snap);
  mpu_snap.struct_version = FABRIC_ABI_VERSION_CURRENT;
  mpu_snap.lamps = mpu_lamps.data(); mpu_snap.lamp_capacity = 320;
  mpu_snap.reels = mpu_reels.data(); mpu_snap.reel_capacity = 8;
  mpu_snap.character_displays = mpu_alpha.data();
  mpu_snap.character_display_capacity = 2;
  mpu_snap.segment_displays = mpu_segments.data();
  mpu_snap.segment_display_capacity = 40;
  CHECK(FabricSessionGetSnapshot(session, &mpu_snap) == FABRIC_OK);
  CHECK(mpu_snap.lamp_count == 320 && mpu_snap.character_display_count == 2);
  CHECK(mpu_snap.segment_display_count == 40);
  CHECK(mpu_reels[0].position == 16000);
  CHECK(mpu_segments[39].segment_masks[0] == 39);
  FabricInput mpu_coin{};
  mpu_coin.struct_size = sizeof(mpu_coin);
  mpu_coin.struct_version = FABRIC_ABI_VERSION_CURRENT;
  mpu_coin.kind = FABRIC_INPUT_COIN; mpu_coin.coin_channel = 2;
  mpu_coin.coin_value = 4; mpu_coin.active = 1;
  CHECK(FabricSessionSubmitInput(session, &mpu_coin) == FABRIC_OK);
  CHECK(FabricSessionSubmitInput(session, &mpu_coin) == FABRIC_OK);
  mpu_coin.active = 0;
  CHECK(FabricSessionSubmitInput(session, &mpu_coin) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &mpu_snap) == FABRIC_OK);
  CHECK(mpu_lamps[5].brightness == 1.0f);
  CHECK(mpu_lamps[6].brightness == 36.0f); /* mechanism 0, channel 2, value 4 */
  CHECK(FabricSessionShutdown(session) == FABRIC_OK);
  FabricDestroySession(session);

  /* A requested section resolves and applies only its own native exports. */
  auto percentage_only = mpu5_configuration();
  percentage_only.flags = FABRIC_AMBER_MPU5_CONFIGURE_PERCENTAGE;
  auto percentage_request = request(FAKE_PRODUCTION_AMBER_MPU5_MISSING_STEPS_PATH);
  std::strcpy(percentage_request.machine_identifier, "barcrest-mpu5");
  percentage_request.rom_resources = roms;
  percentage_request.rom_resource_count = 3;
  percentage_request.machine_configuration = &percentage_only;
  percentage_request.machine_configuration_size = sizeof(percentage_only);
  CHECK(FabricCreateSession(runtime, &percentage_request, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &mpu_snap) == FABRIC_OK);
  CHECK(mpu_lamps[12].brightness == 0.0f &&
        mpu_lamps[15].brightness == 0.0f);
  CHECK(mpu_lamps[18].brightness == 9.0f);
  CHECK(FabricSessionShutdown(session) == FABRIC_OK);
  FabricDestroySession(session);

  auto missing_steps_request = percentage_request;
  auto reels_only = mpu5_configuration();
  reels_only.flags = FABRIC_AMBER_MPU5_CONFIGURE_REELS;
  missing_steps_request.machine_configuration = &reels_only;
  CHECK(FabricCreateSession(runtime, &missing_steps_request, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_NOT_SUPPORTED);
  CHECK(session_error(session).find("SetSteps") != std::string::npos);
  CHECK(session_error(session).find("barcrest-mpu5") != std::string::npos);
  FabricDestroySession(session);

  auto configured_mpu5 = mpu5;
  configured_mpu5.machine_configuration = &mpu_config;
  configured_mpu5.machine_configuration_size = sizeof(mpu_config);
  CHECK(FabricCreateSession(runtime, &configured_mpu5, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &mpu_snap) == FABRIC_OK);
  CHECK(mpu_reels[0].position == 0); /* reel zero was not selected */
  CHECK(mpu_lamps[12].brightness == 7.0f);
  CHECK(mpu_lamps[13].brightness == 11.0f);
  CHECK(mpu_lamps[14].brightness == 1.0f);
  CHECK(mpu_lamps[15].brightness == 1.0f);
  CHECK(mpu_lamps[16].brightness == 5.0f);
  CHECK(mpu_lamps[17].brightness == 1.0f);
  CHECK(mpu_lamps[18].brightness == 9.0f);
  CHECK(FabricSessionReset(session) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &mpu_snap) == FABRIC_OK);
  CHECK(mpu_lamps[12].brightness == 7.0f &&
        mpu_lamps[16].brightness == 5.0f &&
        mpu_lamps[18].brightness == 9.0f);
  CHECK(FabricSessionShutdown(session) == FABRIC_OK);
  FabricDestroySession(session);
  FabricDestroyRuntime(runtime);
  return 0;
}
