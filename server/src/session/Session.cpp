// Follows the X4 firmware's CardPartyBrowserServer: same seat, ticket and host rules.
#include "Session.h"

#include <stdexcept>
#include <utility>

#include "game/CardPartySentence.h"
#include "net/Messages.h"
#include "session/Codes.h"
#include "session/PlayerWords.h"

namespace caw {
using namespace cardparty;

namespace {
constexpr Rules kDefaultRules{7, 10, true};

bool isPlayerMove(Command::Type type) {
  switch (type) {
    case Command::Type::Submit:
    case Command::Type::Judge:
    case Command::Type::Swap:
    case Command::Type::Pause:
    case Command::Type::Unpause:
    case Command::Type::Again:
      return true;
    default:
      return false;
  }
}
}  // namespace

Session::Session(std::string hostToken, PackLibrary& library, std::vector<std::string> packs,
                 std::uint32_t seed, std::uint32_t now)
    : hostToken_(std::move(hostToken)),
      library_(library),
      packIds_(PackLibrary::normalized(packs)) {
  const Deck* deck = library_.deck(packIds_);
  if (deck == nullptr) throw std::invalid_argument("Packs do not form a deck");
  deck_ = *deck;
  if (!game_.configure(deck_, kDefaultRules, seed))
    throw std::invalid_argument("Deck cannot run a match");
  for (ConnectionId& connection : seatConnection_) connection = kNoConnection;
  table_.apply(Settings{}, now);
}

void Session::handle(ConnectionId connection, const Command& command, std::uint32_t now) {
  if (command.type == Command::Type::Join) return join(connection, command, now);
  if (command.type == Command::Type::Resume) return resume(connection, command, now);
  if (command.type == Command::Type::Host) return claimHost(connection, command, now);

  if (isPlayerMove(command.type)) {
    if (!playerMove(connection, command, now)) {
      send(connection, Protocol::error("rejected"));
      // Resync the sender so its controls match the current revision and phase.
      unsigned char secret[kTicketBytes] = {};
      if (Codes::fromHex(command.token, secret, kTicketBytes)) {
        const std::uint8_t seat = seats_.authenticatedSeat(connection, secret);
        if (seat != kNone) sendState(connection, seat, now);
      }
      return;
    }
  } else {
    if (hosts_.count(connection) == 0) return send(connection, Protocol::error("not_host"));
    if (!hostCommand(command, now)) return send(connection, Protocol::error("rejected"));
  }
  table_.tick(now);
  publishAll(now);
}

void Session::join(ConnectionId connection, const Command& command, std::uint32_t now) {
  if (locked_) return send(connection, Protocol::error("locked"));
  unsigned char secret[kTicketBytes] = {};
  Codes::ticket(secret);
  SessionTicket ticket{};
  if (!seats_.join(connection, command.name.c_str(), secret, ticket))
    return send(connection, Protocol::error(joinError(connection)));
  tickets_[ticket.seat] = ticket;
  seatConnection_[ticket.seat] = connection;
  assignIdentity(ticket.seat, command.player);
  send(connection, Protocol::welcome(Codes::toHex(ticket.secret, kTicketBytes), ticket.seat));
  table_.tick(now);
  publishAll(now);
}

// From the firmware's CardPartyBrowserServer::assignId: a missing id or one already at
// the table gets a fresh one, and the word moves on until nobody else here has it.
void Session::assignIdentity(std::uint8_t seat, const std::string& requested) {
  bool taken = requested.empty();
  for (std::uint8_t i = 0; i < kPlayers; ++i)
    if (i != seat && identities_[i].playerId == requested) taken = true;
  const std::string id = taken ? Codes::playerId() : requested;
  std::uint8_t word = wordForPlayer(id);
  bool inUse = true;
  while (inUse) {
    inUse = false;
    for (std::uint8_t i = 0; i < kPlayers; ++i)
      if (i != seat && !identities_[i].playerId.empty() && identities_[i].word == word)
        inUse = true;
    if (inUse) word = static_cast<std::uint8_t>((word + 1) % kPlayerWords);
  }
  identities_[seat] = {id, word};
}

void Session::resume(ConnectionId connection, const Command& command, std::uint32_t now) {
  unsigned char oldSecret[kTicketBytes] = {};
  unsigned char freshSecret[kTicketBytes] = {};
  Codes::ticket(freshSecret);
  SessionTicket ticket{};
  if (!Codes::fromHex(command.token, oldSecret, kTicketBytes) ||
      !seats_.resume(connection, oldSecret, freshSecret, ticket))
    return send(connection, Protocol::notice("expired"));
  const ConnectionId previous = seatConnection_[ticket.seat];
  tickets_[ticket.seat] = ticket;
  seatConnection_[ticket.seat] = connection;
  send(connection, Protocol::welcome(Codes::toHex(ticket.secret, kTicketBytes), ticket.seat));
  // A reloaded tab took the seat over: the old socket is closed, not left dangling.
  if (previous != kNoConnection && previous != connection)
    send(previous, Protocol::notice("replaced"), true);
  table_.tick(now);
  publishAll(now);
}

void Session::claimHost(ConnectionId connection, const Command& command, std::uint32_t now) {
  if (!isHostToken(command.token)) return send(connection, Protocol::error("not_host"));
  hosts_.insert(connection);
  send(connection, Protocol::notice("host"));
  // Notify everyone: a host who already plays must receive the crown on
  // other clients without waiting for the next round or player action.
  publishAll(now);
}

bool Session::playerMove(ConnectionId connection, const Command& command, std::uint32_t now) {
  unsigned char secret[kTicketBytes] = {};
  if (!Codes::fromHex(command.token, secret, kTicketBytes)) return false;
  // Paused or waiting for a missing player: nobody moves.
  const bool frozen = table_.frozen();
  switch (command.type) {
    case Command::Type::Submit:
      return !frozen && seats_.submit(connection, secret, command.revision, command.slots.data(),
                                      static_cast<std::uint8_t>(command.slots.size()));
    case Command::Type::Judge:
      return !frozen && seats_.judge(connection, secret, command.revision, command.index);
    case Command::Type::Swap:
      return !frozen && seats_.swap(connection, secret, command.revision, command.index);
    default:
      break;
  }
  const std::uint8_t seat = seats_.authenticatedSeat(connection, secret);
  if (seat == kNone) return false;
  if (command.type == Command::Type::Pause) return table_.pause(seat, now);
  if (command.type == Command::Type::Unpause) return table_.unpause(seat, now);
  return table_.vote(seat);
}

// The host decides, paused or not, like the X4 does.
bool Session::hostCommand(const Command& command, std::uint32_t now) {
  switch (command.type) {
    case Command::Type::Start:
      return seats_.hostStart();
    case Command::Type::Next:
      return seats_.hostNext();
    case Command::Type::Close:
      return seats_.hostClose();
    case Command::Type::Abort:
      return seats_.hostAbort();
    case Command::Type::Rematch:
      return seats_.hostRematch();
    case Command::Type::Kick:
      return kick(command.index);
    case Command::Type::End:
      if (!game_.endGame()) return false;
      table_.endedByHost();
      return true;
    case Command::Type::HostPause:
      return table_.pause(kHost, now);
    case Command::Type::HostUnpause:
      return table_.unpause(kHost, now);
    case Command::Type::AddTime:
      return table_.addTime(now);
    case Command::Type::ContinueWithout:
      return table_.continueWithout(now);
    case Command::Type::Settings:
      return applySettings(command.settings, now);
    case Command::Type::Packs:
      return selectPacks(command.packs);
    case Command::Type::Lock:
    case Command::Type::Unlock:
      if (locked_ == (command.type == Command::Type::Lock)) return false;
      locked_ = command.type == Command::Type::Lock;
      return true;
    default:
      return false;
  }
}

// Clocks and Rando apply at once, but hand size and round count are
// lobby-only. Reject the whole update before changing either rule.
bool Session::applySettings(const Settings& settings, std::uint32_t now) {
  const bool handChanged = settings.hand != game_.rules().hand;
  const bool roundsChanged = settings.rounds != game_.requestedRounds();
  if (game_.phase() != Phase::Lobby && (handChanged || roundsChanged)) return false;
  if (handChanged && !game_.setHand(settings.hand)) return false;
  if (roundsChanged && !game_.setRounds(settings.rounds)) return false;
  table_.apply(settings, now);
  return true;
}

// Packs change only in the lobby, so no hand or played card points at a vanished card.
bool Session::selectPacks(const std::vector<std::string>& ids) {
  if (game_.phase() != Phase::Lobby) return false;
  const Deck* deck = library_.deck(ids);
  if (deck == nullptr) return false;
  deck_ = *deck;
  if (!game_.reloadDeck()) return false;
  packIds_ = PackLibrary::normalized(ids);
  return true;
}

bool Session::kick(std::uint8_t seat) {
  if (seat >= kPlayers) return false;
  const ConnectionId connection = seatConnection_[seat];
  if (!seats_.hostKick(seat)) return false;
  table_.seatGone(seat);
  tickets_[seat] = SessionTicket{};
  seatConnection_[seat] = kNoConnection;
  identities_[seat] = Identity{};
  // Tell the phone why before its socket goes.
  if (connection != kNoConnection) send(connection, Protocol::notice("kicked"), true);
  return true;
}

std::string Session::joinError(ConnectionId connection) const {
  if (seats_.seatOf(connection) != kNone) return "already_joined";
  if (game_.phase() != Phase::Lobby) return "game_running";
  if (game_.lobbyFull()) return "session_full";
  return "rejected";
}

void Session::addScreen(ConnectionId connection, std::uint32_t now) {
  screens_.insert(connection);
  sendScreen(connection, now);
}

void Session::connectionClosed(ConnectionId connection, std::uint32_t now) {
  hosts_.erase(connection);
  screens_.erase(connection);
  if (!seats_.disconnect(connection)) return;
  for (ConnectionId& seatConnection : seatConnection_)
    if (seatConnection == connection) seatConnection = kNoConnection;
  table_.tick(now);
  publishAll(now);
}

void Session::tick(std::uint32_t now) {
  if (table_.tick(now)) publishAll(now);
}

void Session::resync(ConnectionId connection, std::uint32_t now) {
  const std::uint8_t seat = seats_.seatOf(connection);
  if (seat != kNone || hosts_.count(connection) != 0) sendState(connection, seat, now);
  if (screens_.count(connection) != 0) sendScreen(connection, now);
}

std::vector<Outgoing> Session::takeOutgoing() {
  std::vector<Outgoing> frames;
  frames.swap(outgoing_);
  return frames;
}

std::string Session::saveState(std::uint32_t now) const {
  return Messages::saveState(game_, table_, seats_, now);
}

bool Session::loadState(std::string_view text, std::uint32_t now) {
  if (!Messages::loadState(text, game_, table_, seats_, now)) return false;
  // The saved connection ids belonged to sockets that no longer exist.
  seats_.disconnectAll();
  for (ConnectionId& connection : seatConnection_) connection = kNoConnection;
  for (SessionTicket& ticket : tickets_) ticket = SessionTicket{};
  // The restored state was counted before the restart.
  lastPhase_ = game_.publicView().phase;
  lastRound_ = game_.publicView().round;
  hosts_.clear();
  screens_.clear();
  outgoing_.clear();
  changed_ = true;
  return true;
}

bool Session::takeChanged() {
  const bool changed = changed_;
  changed_ = false;
  return changed;
}

bool Session::isHostToken(std::string_view token) const {
  return Codes::sameToken(hostToken_, token);
}

std::vector<Identity> Session::identities() const {
  return std::vector<Identity>(std::begin(identities_), std::end(identities_));
}

void Session::restoreExtras(const std::vector<Identity>& identities, std::vector<MatchLine> lines,
                            bool locked) {
  locked_ = locked;
  for (std::size_t seat = 0; seat < kPlayers && seat < identities.size(); ++seat)
    identities_[seat] = identities[seat];
  matchLines_ = std::move(lines);
}

std::vector<HistoryEvent> Session::takeHistory() {
  std::vector<HistoryEvent> events;
  events.swap(history_);
  return events;
}

void Session::noteProgress() {
  const PublicView view = game_.publicView();
  if (view.phase == Phase::Playing && lastPhase_ == Phase::Lobby) matchLines_.clear();
  if (view.phase == Phase::Reveal && (lastPhase_ != Phase::Reveal || view.round != lastRound_))
    roundWon(view);
  if (view.phase == Phase::Finished && lastPhase_ != Phase::Finished) matchEnded(view);
  lastPhase_ = view.phase;
  lastRound_ = view.round;
}

std::string Session::playerName(std::uint8_t seat) const {
  return game_.publicView().players[seat].name;
}

// The winning sentence goes into this match's list; a seated winner also into the statistics.
void Session::roundWon(const PublicView& view) {
  if (view.selected >= view.plays || view.prompt >= deck_.promptsCount) return;  // aborted
  const PublicPlay& play = view.pile[view.selected];
  const char* answers[kPickMax] = {"", "", ""};
  for (std::uint8_t i = 0; i < view.pick && i < kPickMax; ++i)
    if (play.cards[i] < deck_.answersCount) answers[i] = deck_.answers[play.cards[i]].text;
  char sentence[1024] = {};
  cardparty::sentence::compose(deck_.prompts[view.prompt].text, answers, view.pick, sentence,
                               sizeof(sentence));
  const bool seated = view.winner < kPlayers;
  matchLines_.push_back({seated ? playerName(view.winner) : "Rando", sentence});
  if (!seated || identities_[view.winner].playerId.empty()) return;
  const Identity& winner = identities_[view.winner];
  history_.push_back({HistoryEvent::Kind::RoundWon, winner.playerId, playerName(view.winner),
                      winner.word, false, sentence});
}

void Session::matchEnded(const PublicView& view) {
  std::uint16_t best = 0;
  for (const PublicPlayer& player : view.players)
    if (player.seated && player.points > best) best = player.points;
  for (std::uint8_t seat = 0; seat < kPlayers; ++seat) {
    const Identity& identity = identities_[seat];
    if (!view.players[seat].seated || identity.playerId.empty()) continue;
    const bool won = best > 0 && view.players[seat].points == best;
    history_.push_back({HistoryEvent::Kind::MatchPlayed, identity.playerId, playerName(seat),
                        identity.word, won, ""});
  }
}

void Session::publishAll(std::uint32_t now) {
  // Every accepted change is published, so this is also the place to mark it for saving.
  changed_ = true;
  noteProgress();
  const StateFrame table = publicFrame(now);
  const std::string shared = Protocol::publicState(table);
  for (std::uint8_t seat = 0; seat < kPlayers; ++seat)
    if (seatConnection_[seat] != kNoConnection) sendState(seatConnection_[seat], seat, shared, now);
  for (ConnectionId host : hosts_)
    if (seats_.seatOf(host) == kNone) sendState(host, kNone, shared, now);
  if (screens_.empty()) return;
  const std::string screen = Protocol::screen(table);
  for (ConnectionId connection : screens_) send(connection, screen);
}

StateFrame Session::publicFrame(std::uint32_t now) const {
  StateFrame frame;
  frame.deck = &deck_;
  frame.table = &table_;
  frame.view = game_.publicView();
  frame.rounds = game_.rules().rounds;
  frame.now = now;
  frame.settings = table_.settings();
  frame.requestedRounds = game_.requestedRounds();
  for (const std::string& id : packIds_) frame.packs.push_back(library_.find(id));
  for (std::uint8_t seat = 0; seat < kPlayers; ++seat) {
    if (!identities_[seat].playerId.empty()) frame.words[seat] = playerWord(identities_[seat].word);
    const ConnectionId connection = seatConnection_[seat];
    frame.hostSeats[seat] = connection != kNoConnection && hosts_.count(connection) != 0;
  }
  frame.lines = &matchLines_;
  frame.locked = locked_;
  return frame;
}

void Session::sendScreen(ConnectionId connection, std::uint32_t now) {
  send(connection, Protocol::screen(publicFrame(now)));
}

void Session::sendState(ConnectionId connection, std::uint8_t seat, std::uint32_t now) {
  sendState(connection, seat, Protocol::publicState(publicFrame(now)), now);
}

void Session::sendState(ConnectionId connection, std::uint8_t seat, const std::string& shared,
                        std::uint32_t now) {
  // Only what "me" needs: the seat, the host flag and the private hand.
  StateFrame own;
  own.table = &table_;
  own.deck = &deck_;
  own.now = now;
  own.seat = seat;
  own.isHost = hosts_.count(connection) != 0;
  if (seat < kPlayers &&
      !seats_.privateHand(connection, tickets_[seat].secret, own.hand, own.revision))
    own.seat = kNone;  // never show a hand the connection cannot prove it owns
  send(connection, Protocol::state(shared, own));
}

void Session::send(ConnectionId connection, std::string text, bool close) {
  outgoing_.push_back({connection, std::move(text), close});
}

}  // namespace caw
