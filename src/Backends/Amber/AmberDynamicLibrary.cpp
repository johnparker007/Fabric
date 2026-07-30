#include "AmberDynamicLibrary.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
namespace fabric {
AmberDynamicLibrary::~AmberDynamicLibrary() {
#ifdef _WIN32
  if (handle_)
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
  if (handle_)
    dlclose(handle_);
#endif
}
bool AmberDynamicLibrary::open(const std::string &path,
                               std::string &error) noexcept {
#ifdef _WIN32
  handle_ = LoadLibraryExA(path.c_str(), nullptr,
                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                               LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if (!handle_) {
    const DWORD code = GetLastError();
    char *message = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, code, 0, reinterpret_cast<char *>(&message), 0,
                   nullptr);
    error = "failed to load exact Amber DLL path (Windows error " +
            std::to_string(code) + ")";
    if (message) {
      error += ": " + std::string(message);
      LocalFree(message);
    }
  }
#else
  handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle_) {
    const char *message = dlerror();
    error = "failed to load exact Amber library path: " +
            std::string(message ? message : "unknown loader error");
  }
#endif
  return handle_ != nullptr;
}
void *AmberDynamicLibrary::symbol(const char *name) noexcept {
#ifdef _WIN32
  return handle_ ? reinterpret_cast<void *>(
                       GetProcAddress(static_cast<HMODULE>(handle_), name))
                 : nullptr;
#else
  return handle_ ? dlsym(handle_, name) : nullptr;
#endif
}
} // namespace fabric
