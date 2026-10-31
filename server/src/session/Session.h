// One running match: rules, seats, clocks and who is connected. No networking in here.
#pragma once

#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "game/CardPartyCore.h"
#include "game/CardPartySessions.h"
#include "game/CardPartyTable.h"
#include "net/Protocol.h"

namespace caw {

using ConnectionId = std::uint32_t;

// A text frame the transport must send. With close set, it closes the socket afterwards.
struct Outgoing {
  ConnectionId connection = 0;
  std::string text;
  bool close = false;
};

class Session final {
 public:
  // The deck must outlive the session. Time is milliseconds from a monotonic clock.
  Session(std::string hostToken, const cardparty::Deck& deck, std::uint32_t seed,
          std::uint32_t now);
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  void handle(ConnectionId connection, const Command& command, std::uint32_t now);
  void connectionClosed(ConnectionId connection, std::uint32_t now);
  // Runs the clocks; call a few times per second.
  void tick(std::uint32_t now);
  // Sends this connection its current state again, e.g. after frames were dropped.
  void resync(ConnectionId connection, std::uint32_t now);

  // Frames produced since the last call, in order.
  std::vector<Outgoing> takeOutgoing();

  // Full internal state for Redis, including tickets and hands. Never send it to a client.
  std::string saveState(std::uint32_t now) const;
  // Restores a saved match. Every seat starts disconnected and waits for its ticket.
  // False leaves the session unchanged.
  bool loadState(std::string_view text, std::uint32_t now);
  // True once after any change worth saving.
  bool takeChanged();

  bool isHostToken(std::string_view token) const;
  std::size_t seatedCount() const { return game_.seatedCount(); }

 private:
  void join(ConnectionId connection, const Command& command, std::uint32_t now);
  void resume(ConnectionId connection, const Command& command, std::uint32_t now);
  void claimHost(ConnectionId connection, const Command& command, std::uint32_t now);
  bool playerMove(ConnectionId connection, const Command& command, std::uint32_t now);
  bool hostCommand(const Command& command, std::uint32_t now);
  bool kick(std::uint8_t seat);
  bool applySettings(const cardparty::Settings& settings, std::uint32_t now);
  std::string joinError(ConnectionId connection) const;

  // Sends the current state to every seat and every host connection.
  void publishAll(std::uint32_t now);
  void sendState(ConnectionId connection, std::uint8_t seat, std::uint32_t now);
  void send(ConnectionId connection, std::string text, bool close = false);

  std::string hostToken_;
  const cardparty::Deck& deck_;
  cardparty::Game game_;
  cardparty::Sessions seats_{game_};
  cardparty::Table table_{game_};
  // Per seat: the socket that holds it and its current ticket.
  ConnectionId seatConnection_[cardparty::kPlayers];
  cardparty::SessionTicket tickets_[cardparty::kPlayers] = {};
  std::set<ConnectionId> hosts_;
  std::vector<Outgoing> outgoing_;
  bool changed_ = false;
};

}  // namespace caw
