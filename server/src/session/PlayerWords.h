// The word after a player's name ("Ada·Toast") that tells two Adas apart.
// List and hash from the X4 firmware (CardPartyBrowserServer.h, CardPartyHistory.h).
#pragma once

#include <cstdint>
#include <string>

namespace caw {

constexpr std::uint8_t kPlayerWords = 64;

// The word for a word number (0–63).
const char* playerWord(std::uint8_t word);
// The word number a player id starts with; the same on every server.
std::uint8_t wordForPlayer(const std::string& playerId);

}  // namespace caw
