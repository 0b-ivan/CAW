// Draw secrets from the operating system, never from the card-game RNG.
#include "Codes.h"

#include <array>
#include <fstream>
#include <stdexcept>

namespace caw {
namespace {
constexpr std::string_view kAlphabet = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";
constexpr std::string_view kHex = "0123456789abcdef";
}  // namespace

void Codes::randomBytes(unsigned char* buffer, std::size_t count) {
  std::ifstream random("/dev/urandom", std::ios::in | std::ios::binary);
  if (!random.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(count)))
    throw std::runtime_error("Operating system entropy unavailable");
}

std::string Codes::sessionCode() {
  std::string code;
  code.reserve(6);
  // 248 is exactly divisible by 31, so rejecting the remaining 8 values
  // avoids the modulo bias that would otherwise favor certain room codes.
  while (code.size() < 6) {
    unsigned char byte = 0;
    randomBytes(&byte, 1);
    if (byte < 248) code.push_back(kAlphabet[byte % kAlphabet.size()]);
  }
  return code;
}

std::string Codes::hostToken() {
  std::array<unsigned char, 16> bytes{};
  randomBytes(bytes.data(), bytes.size());
  return toHex(bytes.data(), bytes.size());
}

void Codes::ticket(unsigned char out[16]) { randomBytes(out, 16); }

std::uint32_t Codes::seed() {
  unsigned char bytes[4] = {};
  randomBytes(bytes, sizeof(bytes));
  return static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 |
         static_cast<std::uint32_t>(bytes[2]) << 16 | static_cast<std::uint32_t>(bytes[3]) << 24;
}

std::string Codes::toHex(const unsigned char* bytes, std::size_t count) {
  std::string text;
  text.reserve(count * 2);
  for (std::size_t i = 0; i < count; ++i) {
    text.push_back(kHex[bytes[i] >> 4]);
    text.push_back(kHex[bytes[i] & 15]);
  }
  return text;
}

bool Codes::fromHex(std::string_view text, unsigned char* out, std::size_t count) {
  if (text.size() != count * 2) return false;
  for (std::size_t i = 0; i < count; ++i) {
    const auto high = kHex.find(text[2 * i]);
    const auto low = kHex.find(text[2 * i + 1]);
    if (high == std::string_view::npos || low == std::string_view::npos) return false;
    out[i] = static_cast<unsigned char>(high * 16 + low);
  }
  return true;
}

bool Codes::validSessionCode(std::string_view code) {
  if (code.size() != 6) return false;
  for (char character : code)
    if (kAlphabet.find(character) == std::string_view::npos) return false;
  return true;
}

bool Codes::validHostToken(std::string_view token) {
  if (token.size() != 32) return false;
  for (char character : token)
    if (kHex.find(character) == std::string_view::npos) return false;
  return true;
}

bool Codes::sameToken(std::string_view left, std::string_view right) {
  if (!validHostToken(left) || !validHostToken(right)) return false;
  unsigned char difference = 0;
  for (std::size_t i = 0; i < left.size(); ++i)
    difference |= static_cast<unsigned char>(left[i] ^ right[i]);
  return difference == 0;
}

}  // namespace caw
