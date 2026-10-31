// Internal state serialization only; public game frames will be added in M2.
#pragma once
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>

#include "game/CardPartySessions.h"
#include "game/CardPartyTable.h"

namespace caw {
class Messages {
 public:
  // Contains private hands and tickets: only pass this snapshot to internal persistence.
  static std::string saveState(const cardparty::Game& game, const cardparty::Table& table,
                               const cardparty::Sessions& seats, std::uint32_t now);
  // Requires the same immutable deck and reattached blocker; failure leaves all targets intact.
  static bool loadState(std::string_view text, cardparty::Game& game, cardparty::Table& table,
                        cardparty::Sessions& seats, std::uint32_t now);

 private:
  static nlohmann::json saveGame(const cardparty::Game& game);
  static void loadGame(const nlohmann::json& state, cardparty::Game& game);
  static nlohmann::json saveTable(const cardparty::Table& table, std::uint32_t now);
  static void loadTable(const nlohmann::json& state, cardparty::Table& table, std::uint32_t now);
  static nlohmann::json saveSeats(const cardparty::Sessions& seats);
  static void loadSeats(const nlohmann::json& state, cardparty::Sessions& seats);
  static void copyTable(const cardparty::Table& source, cardparty::Table& target);
};
}  // namespace caw
