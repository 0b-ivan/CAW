// What a match tells the statistics: a round won with its sentence, or a match played.
#pragma once

#include <cstdint>
#include <string>

namespace caw {

struct HistoryEvent {
  enum class Kind { RoundWon, MatchPlayed };
  Kind kind = Kind::RoundWon;
  std::string playerId;  // public label of the device, never sent to other clients
  std::string name;
  std::uint8_t word = 0;
  bool wonMatch = false;  // MatchPlayed: had the most points
  std::string sentence;   // RoundWon: the winning sentence as plain text
};

}  // namespace caw
