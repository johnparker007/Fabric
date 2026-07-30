#include "AmberTrace.h"
#include "fabric/fabric.h"

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#define CHECK(x)                                                               \
  do {                                                                         \
    if (!(x)) {                                                                \
      std::cerr << "check failed line " << __LINE__ << ": " #x "\n";           \
      return 1;                                                                \
    }                                                                          \
  } while (0)

static void environment(const char *name, const std::string &value) {
#ifdef _WIN32
  _putenv_s(name, value.c_str());
#else
  if (value.empty())
    unsetenv(name);
  else
    setenv(name, value.c_str(), 1);
#endif
}
static uint32_t pid() {
#ifdef _WIN32
  return static_cast<uint32_t>(_getpid());
#else
  return static_cast<uint32_t>(getpid());
#endif
}
static std::string read(const std::filesystem::path &p) {
  std::ifstream f(p, std::ios::binary);
  return {std::istreambuf_iterator<char>(f), {}};
}

int main(int argc, char **argv) {
  using fabric::amber_trace::ParseEnabledValue;
  for (const char *v : {"1", "true", "TRUE", " yes ", "On"})
    CHECK(ParseEnabledValue(v));
  for (const char *v : {"", "0", "false", " no ", "off", "maybe"})
    CHECK(!ParseEnabledValue(v));
  CHECK(argc == 2);
  const std::string mode = argv[1];
  std::filesystem::path path;
  if (mode == "explicit") {
    path = std::filesystem::temp_directory_path() /
           ("fabric-trace-explicit-" + std::to_string(pid()) + ".log");
    std::filesystem::remove(path);
    environment("FABRIC_AMBER_TRACE", " true ");
    environment("FABRIC_AMBER_TRACE_FILE", path.string());
  } else if (mode == "default") {
    path = std::filesystem::temp_directory_path() /
           ("fabric-amber-" + std::to_string(pid()) + ".log");
    std::filesystem::remove(path);
    environment("FABRIC_AMBER_TRACE", "yes");
    environment("FABRIC_AMBER_TRACE_FILE", "");
  } else if (mode == "disabled") {
    path = std::filesystem::temp_directory_path() /
           ("fabric-trace-disabled-" + std::to_string(pid()) + ".log");
    std::filesystem::remove(path);
    environment("FABRIC_AMBER_TRACE", "off");
    environment("FABRIC_AMBER_TRACE_FILE", path.string());
  } else {
    environment("FABRIC_AMBER_TRACE", "1");
    environment("FABRIC_AMBER_TRACE_FILE", "relative/not-allowed.log");
  }
  FabricRuntime *runtime = nullptr;
  CHECK(FabricCreateRuntime(FABRIC_ABI_VERSION_1, &runtime) == FABRIC_OK);
  if (mode == "explicit") {
    FabricLaunchRequest request{};
    request.struct_size = sizeof(request);
    request.struct_version = FABRIC_ABI_VERSION_1;
    std::strcpy(request.backend_kind, "amber");
    std::strcpy(request.machine_identifier, "jpm-system6");
    std::strncpy(request.backend_path, FAKE_PRODUCTION_AMBER_PATH,
                 sizeof(request.backend_path) - 1);
    FabricMachineSession *session = nullptr;
    CHECK(FabricCreateSession(runtime, &request, &session) == FABRIC_OK);
    FabricDestroySession(session);
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i)
      threads.emplace_back([i] {
        for (int j = 0; j < 10; ++j)
          fabric::amber_trace::Write("thread=" + std::to_string(i) +
                                     " line=" + std::to_string(j));
      });
    for (auto &t : threads)
      t.join();
  }
  if (mode == "explicit" || mode == "default") {
    const auto text = read(path);
    CHECK(text.find("FabricCreateRuntime entered") != std::string::npos);
    CHECK(text.find("Fabric runtime module:") != std::string::npos);
    if (mode == "explicit") {
      CHECK(text.find("thread=3 line=9") != std::string::npos);
      CHECK(text.find("selected adapter: production") !=
            std::string::npos);
    }
  }
  if (mode == "disabled")
    CHECK(!std::filesystem::exists(path));
  FabricDestroyRuntime(runtime);
  if (mode == "explicit" || mode == "default")
    std::filesystem::remove(path);
  return 0;
}
