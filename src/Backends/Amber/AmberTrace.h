#ifndef FABRIC_AMBER_TRACE_H
#define FABRIC_AMBER_TRACE_H

#include <string>

namespace fabric::amber_trace {
bool ParseEnabledValue(const std::string &value) noexcept;
bool Enabled() noexcept;
void Write(const std::string &message) noexcept;
void RuntimeStarted() noexcept;
} // namespace fabric::amber_trace
#endif
