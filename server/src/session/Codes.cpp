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
  std::string token;
  token.reserve(32);
  for (unsigned char byte : bytes) {
    token.push_back(kHex[byte >> 4]);
    token.push_back(kHex[byte & 15]);
  }
  return token;
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

}  // namespace caw
