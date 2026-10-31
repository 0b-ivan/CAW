// What a room keeps beside the game rules: who sits where for the statistics,
// and the winning sentences of the running match. Both go into the snapshot.
#pragma once

#include <cstdint>
#include <string>

namespace caw {

struct Identity {
  std::string playerId;  // the device's public id; empty: seat free
  std::uint8_t word = 0;
};

struct MatchLine {
  std::string name;
  std::string sentence;
};

}  // namespace caw
