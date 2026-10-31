// Small JSON logger; callers must use operational messages without private game data.
#pragma once
#include <string_view>

namespace caw::log {
void setLevel(std::string_view level);
void info(std::string_view message);
void warn(std::string_view message);
void error(std::string_view message);
}  // namespace caw::log
