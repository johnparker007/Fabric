#include "AmberTrace.h"

#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <vector>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fabric::amber_trace {
namespace {
std::mutex mutex;
std::ofstream file;
std::string trace_path;
bool initialised = false;
bool enabled = false;
bool open_failure_reported = false;

#ifdef _WIN32
std::string utf8(const std::wstring &value) {
  if (value.empty())
    return {};
  const int required = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                           static_cast<int>(value.size()),
                                           nullptr, 0, nullptr, nullptr);
  if (required <= 0)
    return {};
  std::string result(static_cast<size_t>(required), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                      result.data(), required, nullptr, nullptr);
  return result;
}
#endif

std::string environment(const char *name) {
#ifdef _WIN32
  const int name_size = MultiByteToWideChar(CP_UTF8, 0, name, -1, nullptr, 0);
  std::vector<wchar_t> wide_name(static_cast<size_t>(name_size));
  MultiByteToWideChar(CP_UTF8, 0, name, -1, wide_name.data(), name_size);
  const DWORD required = GetEnvironmentVariableW(wide_name.data(), nullptr, 0);
  if (!required)
    return {};
  std::vector<wchar_t> value(required);
  return GetEnvironmentVariableW(wide_name.data(), value.data(), required)
             ? utf8(value.data())
             : std::string();
#else
  const char *value = std::getenv(name);
  return value ? value : "";
#endif
}

uint32_t process_id() noexcept {
#ifdef _WIN32
  return GetCurrentProcessId();
#else
  return static_cast<uint32_t>(getpid());
#endif
}

void debugger(const std::string &line) noexcept {
#ifdef _WIN32
  OutputDebugStringA(line.c_str());
#else
  (void)line;
#endif
}

std::string timestamp() {
  const auto now = std::chrono::system_clock::now();
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          now.time_since_epoch()) %
      1000;
  const std::time_t value = std::chrono::system_clock::to_time_t(now);
  std::tm local{};
#ifdef _WIN32
  localtime_s(&local, &value);
#else
  localtime_r(&value, &local);
#endif
  std::ostringstream out;
  out << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3)
      << std::setfill('0') << milliseconds.count();
  return out.str();
}

std::string module_path() {
#ifdef _WIN32
  HMODULE module = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(&mutex), &module))
    return "unavailable";
  std::vector<wchar_t> path(32768);
  const DWORD length =
      GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
  return length ? utf8(std::wstring(path.data(), length)) : "unavailable";
#else
  return "unavailable (non-Windows build)";
#endif
}

std::string executable_path() {
#ifdef _WIN32
  std::vector<wchar_t> path(32768);
  const DWORD length =
      GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
  return length ? utf8(std::wstring(path.data(), length)) : "unavailable";
#else
  return "unavailable (non-Windows build)";
#endif
}

void initialise_locked() noexcept {
  if (initialised)
    return;
  initialised = true;
  const std::string trace_value = environment("FABRIC_AMBER_TRACE");
  enabled = ParseEnabledValue(trace_value);
  if (!enabled)
    return;
  try {
    const std::string configured = environment("FABRIC_AMBER_TRACE_FILE");
    std::filesystem::path path;
    if (!configured.empty()) {
      path = std::filesystem::u8path(configured);
      if (!path.is_absolute())
        throw std::runtime_error("trace path is not absolute");
    } else {
      path = std::filesystem::temp_directory_path() /
             ("fabric-amber-" + std::to_string(process_id()) + ".log");
    }
    trace_path = path.u8string();
    file.open(path, std::ios::out | std::ios::app | std::ios::binary);
    if (!file)
      throw std::runtime_error("trace file could not be opened");
    debugger("[Fabric trace] Trace file: " + trace_path + "\n");
  } catch (const std::exception &error) {
    if (!open_failure_reported) {
      debugger(std::string("[Fabric trace] File tracing unavailable: ") +
               error.what() + "\n");
      open_failure_reported = true;
    }
  } catch (...) {
    if (!open_failure_reported) {
      debugger("[Fabric trace] File tracing unavailable: unknown error\n");
      open_failure_reported = true;
    }
  }
}
} // namespace

bool ParseEnabledValue(const std::string &input) noexcept {
  size_t first = 0, last = input.size();
  while (first < last && std::isspace(static_cast<unsigned char>(input[first])))
    ++first;
  while (last > first &&
         std::isspace(static_cast<unsigned char>(input[last - 1])))
    --last;
  std::string value;
  value.reserve(last - first);
  for (size_t i = first; i < last; ++i)
    value.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(input[i]))));
  return value == "1" || value == "true" || value == "yes" || value == "on";
}

bool Enabled() noexcept {
  std::lock_guard<std::mutex> lock(mutex);
  initialise_locked();
  return enabled;
}

void Write(const std::string &message) noexcept {
  try {
    std::lock_guard<std::mutex> lock(mutex);
    initialise_locked();
    if (!enabled)
      return;
    const std::string line = timestamp() + " [pid " +
                             std::to_string(process_id()) + "] [Fabric] " +
                             message + "\n";
    debugger(line);
    if (file) {
      file << line;
      file.flush();
    }
  } catch (...) {
  }
}

void RuntimeStarted() noexcept {
  if (!Enabled())
    return;
  Write("FabricCreateRuntime entered");
  Write("Fabric runtime module: " + module_path());
#ifdef NDEBUG
  Write("Fabric runtime build: Release");
#else
  Write("Fabric runtime build: Debug");
#endif
#if defined(_M_X64) || defined(__x86_64__)
  Write("Fabric runtime architecture: x64");
#else
  Write("Fabric runtime architecture: unknown");
#endif
  Write("Fabric runtime source revision: " FABRIC_SOURCE_REVISION);
  Write("Process executable: " + executable_path());
  Write("Trace file: " + trace_path);
  Write("FABRIC_AMBER_TRACE value: " + environment("FABRIC_AMBER_TRACE"));
  Write("FABRIC_AMBER_TRACE_FILE value: " +
        environment("FABRIC_AMBER_TRACE_FILE"));
}
} // namespace fabric::amber_trace
