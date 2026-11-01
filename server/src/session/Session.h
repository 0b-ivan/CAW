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
#include "packs/PackLibrary.h"
#include "store/HistoryEvent.h"

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
  // The library must outlive the session; `packs` must give a playable deck.
  // Time is milliseconds from a monotonic clock.
  Session(std::string hostToken, PackLibrary& library, std::vector<std::string> packs,
          std::uint32_t seed, std::uint32_t now, std::string customPrefix = {});
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  void handle(ConnectionId connection, const Command& command, std::uint32_t now);
  void connectionClosed(ConnectionId connection, std::uint32_t now);
  // A big screen: gets the public table on every change and cannot send commands.
  void addScreen(ConnectionId connection, std::uint32_t now);
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
  bool inLobby() const;
  // In-progress rounds must finish before a single-instance server restart.
  bool inProgress() const;
  // Activate a just-uploaded room pack and announce the new selection.
  bool activatePacks(const std::vector<std::string>& ids, std::uint32_t now);
  // The chosen packs, sorted: together with saveState() they restore the match.
  const std::vector<std::string>& packs() const { return packIds_; }
  // Seat identities and this match's winning sentences, for the room snapshot.
  std::vector<Identity> identities() const;
  const std::vector<MatchLine>& matchLines() const { return matchLines_; }
  void restoreExtras(const std::vector<Identity>& identities, std::vector<MatchLine> lines,
                     bool locked);
  // The host closed the lobby: nobody new may join until it is opened again.
  bool locked() const { return locked_; }
  // Rounds won and matches played since the last call, for the statistics.
  std::vector<HistoryEvent> takeHistory();
  std::size_t seatedCount() const { return game_.seatedCount(); }

 private:
  void join(ConnectionId connection, const Command& command, std::uint32_t now);
  void resume(ConnectionId connection, const Command& command, std::uint32_t now);
  void claimHost(ConnectionId connection, const Command& command, std::uint32_t now);
  bool playerMove(ConnectionId connection, const Command& command, std::uint32_t now);
  bool hostCommand(const Command& command, std::uint32_t now);
  bool kick(std::uint8_t seat);
  bool applySettings(const cardparty::Settings& settings, std::uint32_t now);
  bool selectPacks(const std::vector<std::string>& ids);
  void assignIdentity(std::uint8_t seat, const std::string& requested);
  // Turns a newly reached reveal or end of match into history events.
  void noteProgress();
  void roundWon(const cardparty::PublicView& view);
  void matchEnded(const cardparty::PublicView& view);
  std::string playerName(std::uint8_t seat) const;
  std::string joinError(ConnectionId connection) const;

  // Sends the current state to every seat and every host connection.
  void publishAll(std::uint32_t now);
  void sendState(ConnectionId connection, std::uint8_t seat, std::uint32_t now);
  // The same with the public part already serialized (Protocol::publicState).
  void sendState(ConnectionId connection, std::uint8_t seat, const std::string& shared,
                 std::uint32_t now);
  void sendScreen(ConnectionId connection, std::uint32_t now);
  StateFrame publicFrame(std::uint32_t now) const;
  void send(ConnectionId connection, std::string text, bool close = false);

  std::string hostToken_;
  std::string customPrefix_;
  PackLibrary& library_;
  std::vector<std::string> packIds_;
  // A copy of the library's deck for the chosen packs; the game points at this one,
  // so a pack switch only has to replace it.
  cardparty::Deck deck_;
  cardparty::Game game_;
  cardparty::Sessions seats_{game_};
  cardparty::Table table_{game_};
  // Per seat: the socket that holds it and its current ticket.
  ConnectionId seatConnection_[cardparty::kPlayers];
  cardparty::SessionTicket tickets_[cardparty::kPlayers] = {};
  std::set<ConnectionId> hosts_;
  std::set<ConnectionId> screens_;
  std::vector<Outgoing> outgoing_;
  Identity identities_[cardparty::kPlayers];
  std::vector<MatchLine> matchLines_;
  std::vector<HistoryEvent> history_;
  // The last phase and round seen, so each reveal and each end counts once.
  cardparty::Phase lastPhase_ = cardparty::Phase::Lobby;
  std::uint16_t lastRound_ = 0;
  bool changed_ = false;
  bool locked_ = false;
};

}  // namespace caw
