#include "fabric/fabric.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << "check failed line " << __LINE__ << ": " #x "\n";           \
      return false;                                                            \
    }                                                                          \
  } while (0)

static bool exercise(const char *path, uint32_t expected_rate,
                     uint16_t expected_channels) {
  FabricRuntime *runtime = nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_1, &runtime) == FABRIC_OK);
  const char *roms[] = {"even.rom", "odd.rom"};
  FabricLaunchRequest request{};
  request.struct_size = sizeof(request);
  request.struct_version = FABRIC_ABI_VERSION_1;
  std::strcpy(request.backend_kind, "amber");
  std::strcpy(request.machine_identifier, "jpm-system6");
  std::strncpy(request.backend_path, path, sizeof(request.backend_path) - 1);
  request.rom_paths = roms;
  request.rom_path_count = 2;
  FabricMachineSession *session = nullptr;
  CHECK(FabricCreateSession(runtime, &request, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_OK);

  FabricAudioFormat format{};
  format.struct_size = sizeof(format);
  format.struct_version = FABRIC_ABI_VERSION_1;
  CHECK(FabricSessionGetAudioFormat(session, &format) == FABRIC_OK);
  CHECK(format.sample_rate == expected_rate &&
        format.channel_count == expected_channels);

  std::vector<int16_t> samples(512, -1);
  uint32_t written = 99;
  CHECK(FabricSessionReadAudio(session, samples.data(), 200, &written) ==
            FABRIC_OK &&
        written == 0);
  CHECK(FabricSessionAdvance(session, 500000) == FABRIC_OK);
  CHECK(FabricSessionReadAudio(session, samples.data(), 200, &written) ==
            FABRIC_OK &&
        written == 0);
  CHECK(FabricSessionAdvance(session, 500000) == FABRIC_OK);
  const uint32_t first_tick = expected_rate / 1000u;
  CHECK(FabricSessionReadAudio(session, samples.data(), 10, &written) ==
            FABRIC_OK &&
        written == 10);
  CHECK(samples[10u * expected_channels - 1u] != -1);
  CHECK(samples[10u * expected_channels] == -1); /* frames, not elements */
  CHECK(FabricSessionReadAudio(session, samples.data(), 200, &written) ==
            FABRIC_OK &&
        written == first_tick - 10u);

  uint32_t ten_tick_frames = first_tick;
  for (uint32_t tick = 1; tick < 10; ++tick) {
    CHECK(FabricSessionAdvance(session, 1000000) == FABRIC_OK);
    CHECK(FabricSessionReadAudio(session, samples.data(), 200, &written) ==
          FABRIC_OK);
    ten_tick_frames += written;
  }
  CHECK(ten_tick_frames == expected_rate / 100u); /* exact 10 ms total */

  CHECK(FabricSessionAdvance(session, 5000000) == FABRIC_OK);
  CHECK(FabricSessionReadAudio(session, samples.data(), 200, &written) ==
        FABRIC_OK);
  CHECK(written == (expected_rate * 3u) / 1000u); /* three-tick catch-up */

  CHECK(FabricSessionAdvance(session, 30000000) == FABRIC_OK);
  CHECK(FabricSessionAdvance(session, 30000000) == FABRIC_OK);
  CHECK(FabricSessionReadAudio(session, samples.data(), 200, &written) ==
        FABRIC_OK);
  CHECK(written == (expected_rate * 3u + 999u) / 1000u); /* bounded queue */
  CHECK(FabricSessionReadAudio(session, samples.data(), 200, &written) ==
            FABRIC_OK &&
        written == 0);

  CHECK(FabricSessionShutdown(session) == FABRIC_OK);
  FabricDestroySession(session);
  FabricDestroyRuntime(runtime);
  return true;
}

static bool partial_native_read() {
  FabricRuntime *runtime = nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_1, &runtime) == FABRIC_OK);
  const char *roms[] = {"even.rom", "odd.rom"};
  FabricLaunchRequest request{};
  request.struct_size = sizeof(request);
  request.struct_version = FABRIC_ABI_VERSION_1;
  std::strcpy(request.backend_kind, "amber");
  std::strcpy(request.machine_identifier, "jpm-system6");
  std::strncpy(request.backend_path, FAKE_AMBER_PARTIAL_PATH,
               sizeof(request.backend_path) - 1);
  request.rom_paths = roms;
  request.rom_path_count = 2;
  FabricMachineSession *session = nullptr;
  CHECK(FabricCreateSession(runtime, &request, &session) == FABRIC_OK);
  CHECK(FabricSessionInitialise(session) == FABRIC_OK);
  FabricAudioFormat format{sizeof(format), FABRIC_ABI_VERSION_1};
  CHECK(FabricSessionGetAudioFormat(session, &format) == FABRIC_OK);
  CHECK(FabricSessionAdvance(session, 1000000) == FABRIC_OK);
  int16_t samples[96]{};
  uint32_t written = 0, total = 0;
  do {
    CHECK(FabricSessionReadAudio(session, samples, 48, &written) == FABRIC_OK);
    CHECK(written <= 7);
    total += written;
  } while (written);
  CHECK(total == 48); /* Short native fills deplete, never duplicate, budget. */
  CHECK(FabricSessionShutdown(session) == FABRIC_OK);
  FabricDestroySession(session);
  FabricDestroyRuntime(runtime);
  return true;
}

int main() {
  return exercise(FAKE_AMBER_441_STEREO_PATH, 44100, 2) &&
                 exercise(FAKE_AMBER_48_STEREO_PATH, 48000, 2) &&
                 exercise(FAKE_AMBER_48_MONO_PATH, 48000, 1) &&
                 partial_native_read()
             ? 0
             : 1;
}
