// Emit one escaped JSON record per line for Docker's log collector.
#include "log.h"

#include <iostream>
#include <string>

namespace caw::log {
namespace {
int minimumLevel = 0;
std::string escaped(std::string_view input) {
  std::string output;
  constexpr char hex[] = "0123456789abcdef";
  for (unsigned char c : input) {
    if (c == '"' || c == '\\') output += '\\';
    if (c < 32) {
      output += "\\u00";
      output += hex[c >> 4];
      output += hex[c & 15];
    } else {
      output += static_cast<char>(c);
    }
  }
  return output;
}
void write(int priority, std::string_view level, std::string_view message) {
  if (priority < minimumLevel) return;
  std::cout << "{\"level\":\"" << level << "\",\"message\":\"" << escaped(message) << "\"}"
            << std::endl;
}
}  // namespace
void setLevel(std::string_view level) {
  minimumLevel = level == "error" ? 2 : (level == "warn" ? 1 : 0);
}
void info(std::string_view message) { write(0, "info", message); }
void warn(std::string_view message) { write(1, "warn", message); }
void error(std::string_view message) { write(2, "error", message); }
}  // namespace caw::log
