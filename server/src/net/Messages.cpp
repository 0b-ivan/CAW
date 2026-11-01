// Versioned, pointer-free snapshots. JSON is confined to this translation unit.
#include "Messages.h"

#include <algorithm>
#include <iterator>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace caw {
namespace {
using nlohmann::json;
using namespace cardparty;

std::uint64_t number(const json& value, std::uint64_t maximum) {
  if ((!value.is_number_unsigned() && !value.is_number_integer()) ||
      (value.is_number_integer() && !value.is_number_unsigned() && value.get<std::int64_t>() < 0) ||
      value.get<std::uint64_t>() > maximum)
    throw std::runtime_error("Invalid snapshot number");
  return value.get<std::uint64_t>();
}
void arraySize(const json& value, std::size_t size) {
  if (!value.is_array() || value.size() != size) throw std::runtime_error("Invalid snapshot array");
}
std::uint8_t seatNumber(const json& value, bool host = false) {
  const auto seat = number(value, kNone);
  if (seat >= kPlayers && seat != kNone && !(host && seat == kHost))
    throw std::runtime_error("Invalid snapshot seat");
  return static_cast<std::uint8_t>(seat);
}
std::uint16_t cardNumber(const json& value, std::uint16_t count) {
  const auto card = number(value, kNoCard);
  if (card != kNoCard && card >= count) throw std::runtime_error("Invalid snapshot card");
  return static_cast<std::uint16_t>(card);
}
// This is an identity check for the immutable deck, not a security hash.
std::uint64_t deckIdentity(const Deck& deck) {
  std::uint64_t hash = UINT64_C(14695981039346656037);
  const auto add = [&](std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) {
      hash ^= (value >> (8 * i)) & 255;
      hash *= UINT64_C(1099511628211);
    }
  };
  const auto text = [&](const char* value) {
    for (; *value; ++value) add(static_cast<unsigned char>(*value));
    add(0);
  };
  add(deck.promptsCount);
  add(deck.answersCount);
  for (std::uint16_t i = 0; i < deck.promptsCount; ++i) {
    text(deck.prompts[i].text);
    add(deck.prompts[i].pick);
    add(deck.prompts[i].draw);
    add(deck.promptMasks ? deck.promptMasks[i] : 0);
  }
  for (std::uint16_t i = 0; i < deck.answersCount; ++i) {
    text(deck.answers[i].text);
    add(deck.answerMasks ? deck.answerMasks[i] : 0);
  }
  return hash;
}
}  // namespace

std::optional<std::string> Messages::createLanguage(std::string_view body) {
  if (body.empty() || body.size() > 1024) return std::nullopt;
  try {
    const json request = json::parse(body);
    if (!request.is_object() || request.size() > 1) return std::nullopt;
    if (request.empty()) return "de";
    if (!request.contains("language")) return std::nullopt;
    if (!request.at("language").is_string()) return std::nullopt;
    const std::string language = request.at("language").get<std::string>();
    if (language != "de" && language != "en") return std::nullopt;
    return language;
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

std::string Messages::sessionCreated(std::string_view code, std::string_view hostToken) {
  return json{{"code", std::string(code)}, {"hostToken", std::string(hostToken)}}.dump();
}

std::string Messages::publicRoom(std::string_view code, std::string_view language) {
  json response;
  response["code"] = std::string(code);
  response["language"] = std::string(language);
  response["phase"] = "lobby";
  return response.dump();
}

std::string Messages::errorResponse(std::string_view errorCode) {
  return json{{"error", std::string(errorCode)}}.dump();
}

std::string Messages::fame(const std::vector<FamePlayer>& players,
                           const std::vector<FameLine>& lines) {
  json playerList = json::array();
  for (const FamePlayer& player : players)
    playerList.push_back({{"name", player.name},
                          {"word", player.word},
                          {"games", player.games},
                          {"wins", player.wins},
                          {"rounds", player.rounds}});
  json lineList = json::array();
  for (const FameLine& line : lines)
    lineList.push_back({{"name", line.name}, {"word", line.word}, {"sentence", line.sentence}});
  return json{{"players", playerList}, {"lines", lineList}}.dump();
}

std::string Messages::packCatalog(const std::vector<std::unique_ptr<CardPack>>& packs) {
  json list = json::array();
  for (const auto& pack : packs) {
    list.push_back({{"id", pack->id},
                    {"title", pack->title},
                    {"language", pack->language},
                    {"license", pack->license},
                    {"rightsHolder", pack->rightsHolder},
                    {"sourceUrl", pack->sourceUrl},
                    {"prompts", pack->prompts.size()},
                    {"answers", pack->answers.size()}});
  }
  return json{{"packs", list}}.dump();
}

std::optional<std::vector<PackEntry>> Messages::readPackManifest(std::string_view text) {
  try {
    const json manifest = json::parse(text);
    std::vector<PackEntry> entries;
    for (const json& item : manifest.at("packs")) {
      const auto text = [&item](const char* key) { return item.at(key).get<std::string>(); };
      PackEntry entry{text("id"),      text("file"),         text("title"),    text("language"),
                      text("license"), text("rightsHolder"), text("sourceUrl")};
      for (const std::string* field : {&entry.id, &entry.file, &entry.title, &entry.language,
                                       &entry.license, &entry.rightsHolder, &entry.sourceUrl})
        if (field->empty()) return std::nullopt;
      // A file name, never a path: a manifest only reaches its own directory.
      if (entry.file.find('/') != std::string::npos || entry.file.find("..") != std::string::npos)
        return std::nullopt;
      entries.push_back(entry);
    }
    return entries;
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

std::string Messages::saveRoom(const RoomRecord& room) {
  json identities = json::array();
  for (const Identity& identity : room.identities)
    identities.push_back({{"player", identity.playerId}, {"word", identity.word}});
  json lines = json::array();
  for (const MatchLine& line : room.lines)
    lines.push_back({{"name", line.name}, {"sentence", line.sentence}});
  const std::string head = json{
      {"schema", 4},          {"language", room.language}, {"hostToken", room.hostToken},
      {"packs", room.packs},  {"identities", identities},  {"lines", lines},
      {"locked", room.locked}}.dump();
  // room.state is already JSON text (saveState); spliced in rather than parsed again.
  return head.substr(0, head.size() - 1) + ",\"state\":" + room.state + "}";
}

std::optional<RoomRecord> Messages::loadRoom(std::string_view text) {
  try {
    const json record = json::parse(text);
    // Older schemas lack packs or player identities; such rooms are not restored.
    if (!record.is_object() || record.size() != 8 || record.at("schema") != 4 ||
        !record.at("language").is_string() || !record.at("hostToken").is_string() ||
        !record.at("packs").is_array() || !record.at("state").is_object())
      return std::nullopt;
    RoomRecord room;
    room.language = record.at("language").get<std::string>();
    room.hostToken = record.at("hostToken").get<std::string>();
    room.packs = record.at("packs").get<std::vector<std::string>>();
    room.state = record.at("state").dump();
    room.locked = record.at("locked").get<bool>();
    for (const json& identity : record.at("identities"))
      room.identities.push_back(
          {identity.at("player").get<std::string>(), identity.at("word").get<std::uint8_t>()});
    for (const json& line : record.at("lines"))
      room.lines.push_back(
          {line.at("name").get<std::string>(), line.at("sentence").get<std::string>()});
    return room;
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

json Messages::saveGame(const Game& game) {
  json players = json::array();
  for (const auto& player : game.players_) {
    players.push_back({{"name", player.name},
                       {"seated", player.seated},
                       {"connected", player.connected},
                       {"submitted", player.submitted},
                       {"held", player.held},
                       {"swapped", player.swapped},
                       {"hand", player.hand},
                       {"size", player.size},
                       {"selected", player.selected},
                       {"points", player.points}});
  }
  json pile = json::array();
  for (const auto& play : game.pile_)
    pile.push_back({{"author", play.author}, {"cards", play.cards}});
  return {
      {"rules",
       {{"hand", game.rules_.hand}, {"rounds", game.rules_.rounds}, {"rando", game.rules_.rando}}},
      {"rng", game.rng_},
      {"phase", game.phase_},
      {"count", game.count_},
      {"czar", game.czar_},
      {"pick", game.pick_},
      {"draw", game.draw_},
      {"round", game.round_},
      {"prompt", game.prompt_},
      {"plays", game.plays_},
      {"selected", game.selected_},
      {"winner", game.winner_},
      {"aborted", game.aborted_},
      {"players", players},
      {"pile", pile},
      {"enabled", game.enabled_},
      {"requestedRounds", game.requestedRounds_},
      {"drawnAnswers", game.drawnAnswers_},
      {"drawnPrompts", game.drawnPrompts_}};
}

// Explicit fields keep the on-disk format independent of padding and class layout.
// Longer than the usual function budget because it mirrors one complete game record.
void Messages::loadGame(const json& state, Game& game) {
  const json& rules = state.at("rules");
  game.rules_ = Rules{static_cast<std::uint8_t>(number(rules.at("hand"), 10)),
                      static_cast<std::uint8_t>(number(rules.at("rounds"), 255)),
                      rules.at("rando").get<bool>()};
  if (!Game::validDeck(*game.deck_, game.rules_)) throw std::runtime_error("Invalid rules");
  game.rng_ = number(state.at("rng"), UINT32_MAX);
  if (!game.rng_) throw std::runtime_error("Invalid random state");
  game.phase_ = static_cast<Phase>(number(state.at("phase"), 4));
  game.count_ = number(state.at("count"), kPlayers);
  game.czar_ = seatNumber(state.at("czar"));
  game.pick_ = number(state.at("pick"), kPickMax);
  game.draw_ = number(state.at("draw"), 2);
  game.round_ = number(state.at("round"), UINT16_MAX);
  game.prompt_ = cardNumber(state.at("prompt"), game.deck_->promptsCount);
  game.plays_ = number(state.at("plays"), kPileMax);
  game.selected_ = seatNumber(state.at("selected"));
  const auto winner = number(state.at("winner"), kNone);
  if (winner >= kPlayers && winner != kNone && winner != kRando)
    throw std::runtime_error("Invalid winner");
  game.winner_ = winner;
  game.aborted_ = state.at("aborted").get<bool>();
  game.enabled_ = number(state.at("enabled"), UINT16_MAX);
  game.requestedRounds_ = number(state.at("requestedRounds"), 255);
  const json& players = state.at("players");
  arraySize(players, kPlayers);
  unsigned seated = 0;
  for (unsigned i = 0; i < kPlayers; ++i) {
    auto& player = game.players_[i];
    const json& value = players[i];
    const std::string name = value.at("name").get<std::string>();
    if (name.size() > 23 || name.find('\0') != std::string::npos)
      throw std::runtime_error("Invalid name");
    std::fill(std::begin(player.name), std::end(player.name), 0);
    std::copy(name.begin(), name.end(), player.name);
    player.seated = value.at("seated").get<bool>();
    player.connected = value.at("connected").get<bool>();
    player.submitted = value.at("submitted").get<bool>();
    player.held = value.at("held").get<bool>();
    player.swapped = value.at("swapped").get<bool>();
    player.size = number(value.at("size"), kHandMax);
    player.points = number(value.at("points"), UINT16_MAX);
    arraySize(value.at("hand"), kHandMax);
    arraySize(value.at("selected"), kPickMax);
    for (unsigned j = 0; j < kHandMax; ++j) {
      player.hand[j] = cardNumber(value.at("hand")[j], game.deck_->answersCount);
      if (j < player.size && player.hand[j] == kNoCard)
        throw std::runtime_error("Missing hand card");
    }
    for (unsigned j = 0; j < kPickMax; ++j) {
      const auto slot = number(value.at("selected")[j], kNone);
      if (slot != kNone && slot >= player.size) throw std::runtime_error("Invalid hand slot");
      player.selected[j] = slot;
    }
    if (player.seated) {
      ++seated;
      if (name.empty()) throw std::runtime_error("Missing name");
    }
  }
  if (seated != game.count_) throw std::runtime_error("Invalid seat count");
  arraySize(state.at("pile"), kPileMax);
  for (unsigned i = 0; i < kPileMax; ++i) {
    const json& value = state.at("pile")[i];
    const auto author = number(value.at("author"), kNone);
    if (author >= kPlayers && author != kNone && author != kRando)
      throw std::runtime_error("Invalid author");
    game.pile_[i].author = author;
    arraySize(value.at("cards"), kPickMax);
    for (unsigned j = 0; j < kPickMax; ++j) {
      game.pile_[i].cards[j] = cardNumber(value.at("cards")[j], game.deck_->answersCount);
      if (i < game.plays_ && j < game.pick_ && game.pile_[i].cards[j] == kNoCard)
        throw std::runtime_error("Missing play card");
    }
  }
  arraySize(state.at("drawnAnswers"), Game::kCardWords);
  arraySize(state.at("drawnPrompts"), Game::kCardWords);
  for (unsigned i = 0; i < Game::kCardWords; ++i) {
    game.drawnAnswers_[i] = number(state.at("drawnAnswers")[i], UINT32_MAX);
    game.drawnPrompts_[i] = number(state.at("drawnPrompts")[i], UINT32_MAX);
  }
  if (game.phase_ == Phase::Playing || game.phase_ == Phase::Judging) {
    if (game.czar_ >= kPlayers || !game.players_[game.czar_].seated || !game.round_ ||
        game.prompt_ == kNoCard || game.pick_ != game.deck_->prompts[game.prompt_].pick ||
        game.draw_ != game.deck_->prompts[game.prompt_].draw)
      throw std::runtime_error("Invalid active round");
  }
  if (game.selected_ != kNone && game.selected_ >= game.plays_)
    throw std::runtime_error("Invalid chosen play");
}

json Messages::saveTable(const Table& table, std::uint32_t now) {
  const Settings& settings = table.settings_;
  json offline = json::array();
  for (auto since : table.offlineSince_)
    offline.push_back(since ? json(static_cast<std::uint32_t>(now - since)) : json(nullptr));
  return {{"settings",
           {{"hand", settings.hand},
            {"rounds", settings.rounds},
            {"answerSecs", settings.answerSecs},
            {"extensionSecs", settings.extensionSecs},
            {"jurorSecs", settings.jurorSecs},
            {"autoSecs", settings.autoSecs},
            {"rando", settings.rando}}},
          {"clock", table.clock_},
          {"remaining", table.left(now)},
          {"extended", table.extended_},
          {"ended", table.endedByHost_},
          {"pausedBy", table.pausedBy_},
          {"blockedOn", table.blockedOn_},
          {"votes", table.votes_},
          {"offline", offline},
          {"lastPhase", table.lastPhase_},
          {"lastRound", table.lastRound_}};
}
void Messages::loadTable(const json& state, Table& table, std::uint32_t now) {
  const json& settings = state.at("settings");
  Settings loaded;
  loaded.rounds = static_cast<std::uint8_t>(number(settings.at("rounds"), 255));
  loaded.answerSecs = static_cast<std::uint8_t>(number(settings.at("answerSecs"), 255));
  loaded.extensionSecs = static_cast<std::uint8_t>(number(settings.at("extensionSecs"), 255));
  loaded.jurorSecs = static_cast<std::uint8_t>(number(settings.at("jurorSecs"), 255));
  loaded.autoSecs = static_cast<std::uint8_t>(number(settings.at("autoSecs"), 255));
  loaded.rando = settings.at("rando").get<bool>();
  loaded.hand = table.game_.rules_.hand;
  if (settings.contains("hand"))
    loaded.hand = static_cast<std::uint8_t>(number(settings.at("hand"), 10));
  table.settings_ = loaded;
  // Old Redis snapshots have no hand setting; retain their game's saved
  // hand size instead of silently replacing ten cards with the new default.
  if (table.settings_.hand != table.game_.rules_.hand)
    throw std::runtime_error("Hand setting does not match game rules");
  table.clock_ = static_cast<Table::Clock>(number(state.at("clock"), 3));
  table.left_ = number(state.at("remaining"), INT32_MAX);
  table.deadline_ = now + table.left_;
  table.extended_ = state.at("extended").get<bool>();
  table.endedByHost_ = state.at("ended").get<bool>();
  table.pausedBy_ = seatNumber(state.at("pausedBy"), true);
  table.blockedOn_ = seatNumber(state.at("blockedOn"));
  table.votes_ = number(state.at("votes"), 255);
  table.lastPhase_ = static_cast<Phase>(number(state.at("lastPhase"), 4));
  table.lastRound_ = number(state.at("lastRound"), UINT16_MAX);
  arraySize(state.at("offline"), kPlayers);
  for (unsigned i = 0; i < kPlayers; ++i) {
    const json& elapsed = state.at("offline")[i];
    table.offlineSince_[i] = elapsed.is_null() ? 0 : now - number(elapsed, INT32_MAX);
    if (!elapsed.is_null() && !table.offlineSince_[i]) table.offlineSince_[i] = 1;
  }
}
void Messages::copyTable(const Table& source, Table& target) {
  target.settings_ = source.settings_;
  target.clock_ = source.clock_;
  target.deadline_ = source.deadline_;
  target.left_ = source.left_;
  target.extended_ = source.extended_;
  target.endedByHost_ = source.endedByHost_;
  target.pausedBy_ = source.pausedBy_;
  target.blockedOn_ = source.blockedOn_;
  target.votes_ = source.votes_;
  std::copy(std::begin(source.offlineSince_), std::end(source.offlineSince_), target.offlineSince_);
  target.lastPhase_ = source.lastPhase_;
  target.lastRound_ = source.lastRound_;
}
json Messages::saveSeats(const Sessions& seats) {
  json result = json::array();
  for (const auto& seat : seats.seats_)
    result.push_back({{"registered", seat.registered},
                      {"connected", seat.connected},
                      {"connection", seat.connection},
                      {"revision", seat.revision},
                      {"secret", seat.secret}});
  return result;
}
void Messages::loadSeats(const json& state, Sessions& seats) {
  arraySize(state, kPlayers);
  seats.reset();
  for (unsigned i = 0; i < kPlayers; ++i) {
    auto& seat = seats.seats_[i];
    const json& value = state[i];
    seat.registered = value.at("registered").get<bool>();
    seat.connected = value.at("connected").get<bool>();
    seat.connection = number(value.at("connection"), UINT32_MAX);
    seat.revision = number(value.at("revision"), UINT32_MAX);
    arraySize(value.at("secret"), kTicketBytes);
    for (unsigned j = 0; j < kTicketBytes; ++j) seat.secret[j] = number(value.at("secret")[j], 255);
    if (!seat.registered) {
      if (seat.connected) throw std::runtime_error("Unregistered connection");
      continue;
    }
    if (!seats.game_.players_[i].seated || !Sessions::validSecret(seat.secret) ||
        (seat.connected && seat.connection == kNoConnection))
      throw std::runtime_error("Invalid registered seat");
    for (unsigned j = 0; j < i; ++j) {
      const auto& prior = seats.seats_[j];
      if (prior.registered &&
          (Sessions::sameSecret(prior.secret, seat.secret) ||
           (prior.connected && seat.connected && prior.connection == seat.connection)))
        throw std::runtime_error("Duplicate seat binding");
    }
  }
}
std::string Messages::saveState(const Game& game, const Table& table, const Sessions& seats,
                                std::uint32_t now) {
  if (!game.deck_ || &table.game_ != &game || &seats.game_ != &game)
    throw std::runtime_error("Snapshot requires a configured game and matching owners");
  return json{{"schema", 1},
              {"deck", deckIdentity(*game.deck_)},
              {"game", saveGame(game)},
              {"table", saveTable(table, now)},
              {"seats", saveSeats(seats)}}
      .dump();
}
bool Messages::loadState(std::string_view text, Game& game, Table& table, Sessions& seats,
                         std::uint32_t now) {
  if (!game.deck_ || &table.game_ != &game || &seats.game_ != &game || text.size() > 65536)
    return false;
  try {
    const json state = json::parse(text);
    if (number(state.at("schema"), UINT32_MAX) != 1 ||
        number(state.at("deck"), UINT64_MAX) != deckIdentity(*game.deck_))
      return false;
    Game candidate = game;
    Table candidateTable(candidate);
    Sessions candidateSeats(candidate);
    loadGame(state.at("game"), candidate);
    loadTable(state.at("table"), candidateTable, now);
    loadSeats(state.at("seats"), candidateSeats);
    game = candidate;
    copyTable(candidateTable, table);
    std::copy(std::begin(candidateSeats.seats_), std::end(candidateSeats.seats_), seats.seats_);
    return true;
  } catch (const json::exception&) {
    return false;
  } catch (const std::runtime_error&) {
    return false;
  }
}
}  // namespace caw
