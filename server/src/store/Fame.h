// Rows of the Wall of Fame, as read from Postgres. Never carries a player id.
#pragma once

#include <cstdint>
#include <string>

namespace caw {

struct FamePlayer {
  std::string name;
  std::string word;
  std::uint32_t games = 0;
  std::uint32_t wins = 0;
  std::uint32_t rounds = 0;
};

struct FameLine {
  std::string name;
  std::string word;
  std::string sentence;
};

}  // namespace caw
