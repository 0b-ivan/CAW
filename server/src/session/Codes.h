// Cryptographically generated six-character room codes and host tokens.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace caw {

class Codes final {
 public:
  static std::string sessionCode();
  static std::string hostToken();
  // A player's seat ticket: 16 random bytes, shown to the client as 32 hex characters.
  static void ticket(unsigned char out[16]);
  // Seed for a match's card shuffling, so two rooms never deal the same game.
  static std::uint32_t seed();
  static std::string toHex(const unsigned char* bytes, std::size_t count);
  // False unless the text is exactly 2 * count lowercase hex characters.
  static bool fromHex(std::string_view text, unsigned char* out, std::size_t count);
  static bool validSessionCode(std::string_view code);
  static bool validHostToken(std::string_view token);
  // Constant-time comparison of two well-formed tokens; false if either is malformed.
  static bool sameToken(std::string_view left, std::string_view right);

 private:
  static void randomBytes(unsigned char* buffer, std::size_t count);
};

}  // namespace caw
