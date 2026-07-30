#ifndef FABRIC_AMBER_DYNAMIC_LIBRARY_H
#define FABRIC_AMBER_DYNAMIC_LIBRARY_H
#include <string>
namespace fabric {
class AmberDynamicLibrary {
public:
    ~AmberDynamicLibrary();
    bool open(const std::string &path, std::string &error) noexcept;
    void *symbol(const char *name) noexcept;
private:
    void *handle_ = nullptr;
};
}
#endif
