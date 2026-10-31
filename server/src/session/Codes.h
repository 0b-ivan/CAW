// Cryptographically generated six-character room codes and host tokens.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace caw {

class Codes final {
 public:
  static std::string sessionCode();
  static std::string hostToken();
  static bool validSessionCode(std::string_view code);
  static bool validHostToken(std::string_view token);

 private:
  static void randomBytes(unsigned char* buffer, std::size_t count);
};

}  // namespace caw
