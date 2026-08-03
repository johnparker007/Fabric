#include "fabric/fabric.h"
#include "fabric/fabric_amber.h"

#include <cstddef>
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

int main() {
  CHECK(sizeof(FabricAmberCoinChannelConfigV1) == 20);
  CHECK(offsetof(FabricAmberCoinChannelConfigV1, lockout_value) == 12);
  CHECK(offsetof(FabricAmberCoinChannelConfigV1, lockout_invert) == 16);
  CHECK(sizeof(FabricInput) == 92);
  CHECK(sizeof(FabricAmberCoinConfigurationV2) == 424);
  CHECK(sizeof(FabricAmberConfigurationV2) == 664);
  FabricRuntime *runtime = nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_CURRENT, &runtime) == FABRIC_OK);
  auto bad = request(FAKE_PRODUCTION_AMBER_MISSING_REQUIRED_PATH);
  FabricMachineSession *session = nullptr;
  CHECK(FabricCreateSession(runtime, &bad, &session) == FABRIC_NOT_SUPPORTED);
  CHECK(error(runtime).find("required export 'Initialise'") !=
        std::string::npos);
  auto missing_coin = request(FAKE_PRODUCTION_AMBER_MISSING_COIN_IN_PATH);
  CHECK(FabricCreateSession(runtime, &missing_coin, &session) ==
        FABRIC_NOT_SUPPORTED);
  CHECK(error(runtime).find("required export 'CoinIn'") != std::string::npos);
  auto missing_mechanism =
      request(FAKE_PRODUCTION_AMBER_MISSING_COIN_MECHANISM_PATH);
  CHECK(FabricCreateSession(runtime, &missing_mechanism, &session) ==
        FABRIC_NOT_SUPPORTED);
  CHECK(error(runtime).find("required export 'SetCommStyle'") !=
        std::string::npos);
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
  FabricAmberConfigurationV2 c{};
  c.magic = FABRIC_AMBER_CONFIGURATION_MAGIC;
  c.struct_size = sizeof(c);
  c.version = FABRIC_AMBER_CONFIGURATION_VERSION_2;
  c.flags = FABRIC_AMBER_CONFIGURE_REELS | FABRIC_AMBER_CONFIGURE_COINS |
            FABRIC_AMBER_CONFIGURE_PERCENTAGE;
  c.reels.struct_size = sizeof(c.reels);
  c.reels.version = FABRIC_AMBER_REEL_CONFIGURATION_VERSION_1;
  c.reels.reel_count = 1;
  c.reels.apply_mask = 1;
  c.reels.reels[0].steps = 9;
  c.reels.reels[0].enabled = 1;
  c.coins.struct_size = sizeof(c.coins);
  c.coins.version = FABRIC_AMBER_COIN_CONFIGURATION_VERSION_2;
  c.coins.channel_apply_mask = 0x7;
  c.coins.channels[0] = {0, 1, 4, 0, 1};
  c.coins.channels[1] = {1, 0, 10, 2, 0};
  c.coins.channels[2] = {2, 1, 20, 1, 0};
  /* Populated but deliberately omitted from channel_apply_mask. */
  c.coins.channels[3] = {3, 1, 50, 2, 1};
  c.coins.coin_communication_style =
      FABRIC_AMBER_COIN_COMMUNICATION_PARALLEL;
  c.coins.coin_communication_invert = 0;
  c.coins.coin_pulse_cycles = 800000;
  c.coins.coin_edc_enabled = 0;
  c.percentage_switch = 7;
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
  FabricCapabilities capabilities{sizeof(capabilities),
                                  FABRIC_ABI_VERSION_CURRENT};
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
  auto check_coin_configuration = [&]() {
    const uint32_t expected[3][4] = {
        {4, 1, 0, 1}, {10, 0, 2, 0}, {20, 1, 1, 0}};
    for (uint32_t channel = 0; channel < 3; ++channel)
      for (uint32_t setter = 0; setter < 4; ++setter) {
        const auto &lamp = lamps[16 + channel * 4 + setter];
        CHECK(lamp.logical_state == 1);
        CHECK(lamp.brightness == static_cast<float>(expected[channel][setter]));
      }
    for (uint32_t setter = 0; setter < 4; ++setter)
      CHECK(lamps[16 + 3 * 4 + setter].logical_state == 0);
    const uint32_t mechanism[4] = {FABRIC_AMBER_COIN_COMMUNICATION_PARALLEL,
                                   0, 800000, 0};
    for (uint32_t setter = 0; setter < 4; ++setter) {
      CHECK(lamps[44 + setter].logical_state == 1);
      CHECK(lamps[44 + setter].brightness ==
            static_cast<float>(mechanism[setter]));
    }
    return 0;
  };
  /* Initialise performs the startup Reset and applies all four setters. */
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(check_coin_configuration() == 0);
  const float switch_on_before_coin = lamps[42].brightness;
  const float switch_off_before_coin = lamps[43].brightness;
  input.kind = FABRIC_INPUT_COIN;
  input.coin_channel = 2;
  input.coin_value = 3;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(lamps[48].brightness == 1.0f && lamps[40].brightness == 2.0f);
  CHECK(lamps[41].brightness == 3.0f);
  CHECK(lamps[42].brightness == switch_on_before_coin);
  CHECK(lamps[43].brightness == switch_off_before_coin);
  input.coin_value = 4;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_INPUT_REJECTED);
  input.active = 0;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_OK);
  input.coin_channel = FABRIC_AMBER_MAX_COIN_CHANNELS;
  input.coin_value = 3;
  input.active = 1;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_INVALID_ARGUMENT);
  input.coin_channel = 2;
  input.coin_value = 256;
  CHECK(FabricSessionSubmitInput(session, &input) == FABRIC_INVALID_ARGUMENT);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(lamps[48].brightness == 2.0f && lamps[41].brightness == 4.0f);
  CHECK(lamps[42].brightness == switch_on_before_coin);
  CHECK(lamps[43].brightness == switch_off_before_coin);
  input.kind = FABRIC_INPUT_DIGITAL;
  input.coin_channel = input.coin_value = 0;
  input.active = 1;
  CHECK(FabricSessionReset(session) == FABRIC_OK);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(check_coin_configuration() == 0);
  CHECK(FabricSessionGetSnapshot(session, &snap) == FABRIC_OK);
  CHECK(lamps[0].logical_state == 1 && lamps[1].logical_state == 1 &&
        lamps[1].brightness == 4.0f);
  CHECK(reels[0].position == 9); /* A partial tick performs no native Run. */
  CHECK(chars[0].characters[0] == 0x1234 && chars[0].attributes[0] == 1 &&
        chars[0].brightness == 0.5f);
  CHECK(segments[0].segment_masks[0] == 0x5a);
  CHECK(lamps[3].brightness == 3.0f && lamps[4].brightness >= 1.0f);

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
  bool selected = false, loaded = false, coin_applied = false;
  bool mechanism_configured = false, coin_accepted = false,
       coin_rejected = false;
  for (const auto &message : diagnostics) {
    selected |= message.find("operation=AdapterSelected") != std::string::npos;
    loaded |= message.find("operation=AmberLibraryLoaded") != std::string::npos;
    coin_applied |=
        message.find("operation=AmberCoinChannelApplied") != std::string::npos &&
        message.find("index=2; enabled=1; value=20; lockoutValue=1; "
                     "lockoutInvert=0") != std::string::npos;
    mechanism_configured |=
        message.find("operation=AmberCoinMechanismConfigured") !=
            std::string::npos &&
        message.find("style=parallel; invert=0; cycles=800000; edc=0") !=
            std::string::npos;
    coin_accepted |= message.find("operation=AmberCoinInput") !=
                         std::string::npos &&
                     message.find("channel=2; value=3; result=accepted") !=
                         std::string::npos;
    coin_rejected |= message.find("operation=AmberCoinInput") !=
                         std::string::npos &&
                     message.find("channel=2; value=4; result=rejected") !=
                         std::string::npos;
  }
  CHECK(selected && loaded && coin_applied && mechanism_configured &&
        coin_accepted && coin_rejected);
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
  FabricDestroyRuntime(runtime);
  return 0;
}
