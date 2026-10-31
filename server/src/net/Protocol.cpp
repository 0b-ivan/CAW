// JSON for the live game lives here; snapshots and REST bodies live in Messages.cpp.
#include "Protocol.h"

#include <initializer_list>
#include <nlohmann/json.hpp>

#include "session/Codes.h"

namespace caw {
namespace {
using nlohmann::json;
using namespace cardparty;

// Which fields a command carries besides "type".
enum Field : unsigned {
  kName = 1,
  kToken = 2,
  kRevision = 4,
  kSlots = 8,
  kIndex = 16,
  kSettings = 32,
  kPacks = 64,
  kPlayer = 128
};

struct CommandSpec {
  const char* name;
  Command::Type type;
  unsigned fields;        // required
  unsigned optional = 0;  // may be left out
};

const char* fieldName(unsigned field) {
  switch (field) {
    case kName:
      return "name";
    case kToken:
      return "token";
    case kRevision:
      return "revision";
    case kSlots:
      return "slots";
    case kIndex:
      return "index";
    case kSettings:
      return "settings";
    case kPacks:
      return "packs";
    default:
      return "player";
  }
}

constexpr CommandSpec kCommands[] = {
    {"join", Command::Type::Join, kName, kPlayer},
    {"resume", Command::Type::Resume, kToken},
    {"host", Command::Type::Host, kToken},
    {"submit", Command::Type::Submit, kToken | kRevision | kSlots},
    {"judge", Command::Type::Judge, kToken | kRevision | kIndex},
    {"swap", Command::Type::Swap, kToken | kRevision | kIndex},
    {"pause", Command::Type::Pause, kToken},
    {"unpause", Command::Type::Unpause, kToken},
    {"again", Command::Type::Again, kToken},
    {"start", Command::Type::Start, 0},
    {"next", Command::Type::Next, 0},
    {"close", Command::Type::Close, 0},
    {"abort", Command::Type::Abort, 0},
    {"kick", Command::Type::Kick, kIndex},
    {"end", Command::Type::End, 0},
    {"rematch", Command::Type::Rematch, 0},
    {"hostPause", Command::Type::HostPause, 0},
    {"hostUnpause", Command::Type::HostUnpause, 0},
    {"addTime", Command::Type::AddTime, 0},
    {"continueWithout", Command::Type::ContinueWithout, 0},
    {"settings", Command::Type::Settings, kSettings},
    {"packs", Command::Type::Packs, kPacks},
};

const CommandSpec* findCommand(const std::string& name) {
  for (const CommandSpec& spec : kCommands)
    if (name == spec.name) return &spec;
  return nullptr;
}

bool smallNumber(const json& value, std::uint64_t maximum) {
  return value.is_number_unsigned() && value.get<std::uint64_t>() <= maximum;
}

// One of the listed values; false for anything else, including non-numbers.
bool option(const json& value, std::initializer_list<std::uint8_t> options, std::uint8_t& out) {
  if (!smallNumber(value, 255)) return false;
  for (std::uint8_t candidate : options)
    if (value.get<std::uint8_t>() == candidate) {
      out = candidate;
      return true;
    }
  return false;
}

bool readSettings(const json& value, Settings& settings) {
  if (!value.is_object() || value.size() != 6 || !value.at("rando").is_boolean()) return false;
  settings.rando = value.at("rando").get<bool>();
  // The same choices the X4 offers on its rules screen.
  return option(value.at("rounds"), {3, 5, 8, 10, 12, 15, 20, 30}, settings.rounds) &&
         option(value.at("answerSecs"), {0, 30, 45, 60, 90, 120}, settings.answerSecs) &&
         option(value.at("extensionSecs"), {0, 10, 15, 20, 30}, settings.extensionSecs) &&
         option(value.at("jurorSecs"), {0, 30, 60, 90}, settings.jurorSecs) &&
         option(value.at("autoSecs"), {0, 5, 8, 12}, settings.autoSecs);
}

// Reads one field into the command; false if it is missing or malformed.
bool readField(const json& message, unsigned field, Command& command) {
  switch (field) {
    case kName: {
      const json& name = message.at("name");
      if (!name.is_string() || name.get<std::string>().size() > 23) return false;
      command.name = name.get<std::string>();
      return true;
    }
    case kToken: {
      const json& token = message.at("token");
      if (!token.is_string() || token.get<std::string>().size() != 32) return false;
      command.token = token.get<std::string>();
      return true;
    }
    case kRevision:
      if (!smallNumber(message.at("revision"), UINT32_MAX)) return false;
      command.revision = message.at("revision").get<std::uint32_t>();
      return true;
    case kSlots: {
      const json& slots = message.at("slots");
      if (!slots.is_array() || slots.empty() || slots.size() > kPickMax) return false;
      for (const json& slot : slots) {
        if (!smallNumber(slot, kHandMax - 1)) return false;
        command.slots.push_back(slot.get<std::uint8_t>());
      }
      return true;
    }
    case kIndex:
      if (!smallNumber(message.at("index"), kPlayers + kPileMax)) return false;
      command.index = message.at("index").get<std::uint8_t>();
      return true;
    case kSettings:
      return readSettings(message.at("settings"), command.settings);
    case kPlayer: {
      const json& player = message.at("player");
      if (!player.is_string() || !Codes::validPlayerId(player.get<std::string>())) return false;
      command.player = player.get<std::string>();
      return true;
    }
    case kPacks: {
      // Ids only; whether they exist and make a deck is the library's call.
      const json& packs = message.at("packs");
      if (!packs.is_array() || packs.empty() || packs.size() > 8) return false;
      for (const json& id : packs) {
        if (!id.is_string() || id.get<std::string>().size() > 40) return false;
        command.packs.push_back(id.get<std::string>());
      }
      return true;
    }
    default:
      return false;
  }
}

const char* phaseName(Phase phase) {
  switch (phase) {
    case Phase::Lobby:
      return "lobby";
    case Phase::Playing:
      return "playing";
    case Phase::Judging:
      return "judging";
    case Phase::Reveal:
      return "reveal";
    case Phase::Finished:
      return "finished";
  }
  return "lobby";
}

const char* clockName(Table::Clock clock) {
  switch (clock) {
    case Table::Clock::Answers:
      return "answers";
    case Table::Clock::Juror:
      return "juror";
    case Table::Clock::Next:
      return "next";
    case Table::Clock::None:
      return "";
  }
  return "";
}

std::string seatName(const PublicView& view, std::uint8_t seat) {
  if (seat == kHost) return "host";
  if (seat >= kPlayers || !view.players[seat].seated) return "";
  return view.players[seat].name;
}

json playersJson(const StateFrame& frame) {
  json players = json::array();
  for (std::uint8_t seat = 0; seat < kPlayers; ++seat) {
    const PublicPlayer& player = frame.view.players[seat];
    if (!player.seated) continue;
    players.push_back({{"seat", seat},
                       {"name", player.name},
                       {"word", frame.words[seat] ? frame.words[seat] : ""},
                       {"points", player.points},
                       {"connected", player.connected}});
  }
  return players;
}

json linesJson(const StateFrame& frame) {
  json lines = json::array();
  if (frame.view.phase != Phase::Finished || frame.lines == nullptr) return lines;
  for (const MatchLine& line : *frame.lines)
    lines.push_back({{"name", line.name}, {"sentence", line.sentence}});
  return lines;
}

// The submitted plays, anonymous until the reveal names their authors.
json playsJson(const StateFrame& frame) {
  json plays = json::array();
  const PublicView& view = frame.view;
  if (view.phase != Phase::Judging && view.phase != Phase::Reveal) return plays;
  for (std::uint8_t i = 0; i < view.plays; ++i) {
    json cards = json::array();
    for (std::uint8_t c = 0; c < view.pick; ++c) {
      const std::uint16_t card = view.pile[i].cards[c];
      if (card < frame.deck->answersCount) cards.push_back(frame.deck->answers[card].text);
    }
    json play = {{"cards", cards}};
    if (view.phase == Phase::Reveal) play["author"] = seatName(view, view.pile[i].author);
    plays.push_back(play);
  }
  return plays;
}

json settingsJson(const StateFrame& frame) {
  const Settings& settings = frame.settings;
  return {{"rounds", frame.requestedRounds},         {"answerSecs", settings.answerSecs},
          {"extensionSecs", settings.extensionSecs}, {"jurorSecs", settings.jurorSecs},
          {"autoSecs", settings.autoSecs},           {"rando", settings.rando}};
}

// Title, license and rights holder of every pack in play: shown wherever cards are shown.
json packsJson(const StateFrame& frame) {
  json packs = json::array();
  for (const CardPack* pack : frame.packs) {
    if (pack == nullptr) continue;
    packs.push_back({{"id", pack->id},
                     {"title", pack->title},
                     {"license", pack->license},
                     {"rightsHolder", pack->rightsHolder},
                     {"sourceUrl", pack->sourceUrl}});
  }
  return packs;
}

json ownJson(const StateFrame& frame) {
  json own = {{"seat", frame.seat}, {"isHost", frame.isHost}, {"revision", frame.revision}};
  if (frame.seat >= kPlayers) return own;
  json hand = json::array();
  for (std::uint8_t i = 0; i < frame.hand.size; ++i)
    hand.push_back(frame.deck->answers[frame.hand.cards[i]].text);
  own["hand"] = hand;
  own["isJuror"] = frame.hand.judge;
  own["submitted"] = frame.hand.submitted;
  own["swapped"] = frame.hand.swapped;
  own["voted"] = frame.table->voted(frame.seat);
  own["pausedByMe"] = frame.table->pausedBy() == frame.seat;
  return own;
}
}  // namespace

std::optional<Command> Protocol::parse(std::string_view text) {
  if (text.empty() || text.size() > kMaxMessageBytes) return std::nullopt;
  try {
    const json message = json::parse(text);
    if (!message.is_object() || !message.contains("type") || !message.at("type").is_string())
      return std::nullopt;
    const CommandSpec* spec = findCommand(message.at("type").get<std::string>());
    if (spec == nullptr) return std::nullopt;
    Command command;
    command.type = spec->type;
    std::size_t expectedKeys = 1;
    for (unsigned field : {kName, kToken, kRevision, kSlots, kIndex, kSettings, kPacks, kPlayer}) {
      const bool required = (spec->fields & field) != 0;
      const bool given = (spec->optional & field) != 0 && message.contains(fieldName(field));
      if (!required && !given) continue;
      ++expectedKeys;
      if (!readField(message, field, command)) return std::nullopt;
    }
    // Unknown extra keys are rejected, not ignored.
    if (message.size() != expectedKeys) return std::nullopt;
    return command;
  } catch (const json::exception&) {
    return std::nullopt;
  }
}

namespace {
// Everything anyone at the table may see; state() adds the connection's own part.
json publicState(const StateFrame& frame) {
  const PublicView& view = frame.view;
  const Table& table = *frame.table;
  json state = {{"type", "state"},
                {"phase", phaseName(view.phase)},
                {"round", view.round},
                {"rounds", frame.rounds},
                {"juror", seatName(view, view.czar)},
                {"pick", view.pick},
                {"submitted", view.submitted},
                {"expected", view.expected},
                {"players", playersJson(frame)},
                {"plays", playsJson(frame)},
                {"clock", clockName(table.clock())},
                {"left", table.left(frame.now)},
                {"pausedBy", seatName(view, table.pausedBy())},
                {"blockedOn", seatName(view, table.blockedOn())},
                {"votes", table.votes()},
                {"settings", settingsJson(frame)},
                {"packs", packsJson(frame)},
                {"lines", linesJson(frame)}};
  state["prompt"] =
      view.prompt < frame.deck->promptsCount ? frame.deck->prompts[view.prompt].text : "";
  state["selected"] = view.selected == kNone ? json(nullptr) : json(view.selected);
  state["winner"] = seatName(view, view.winner);
  return state;
}
}  // namespace

std::string Protocol::state(const StateFrame& frame) {
  json state = publicState(frame);
  state["me"] = ownJson(frame);
  return state.dump();
}

std::string Protocol::screen(const StateFrame& frame) {
  json screen = publicState(frame);
  screen["type"] = "screen";
  return screen.dump();
}

std::string Protocol::welcome(std::string_view ticket, std::uint8_t seat) {
  return json{{"type", "welcome"}, {"token", std::string(ticket)}, {"seat", seat}}.dump();
}

std::string Protocol::error(std::string_view code) {
  return json{{"type", "error"}, {"code", std::string(code)}}.dump();
}

std::string Protocol::notice(std::string_view type) {
  return json{{"type", std::string(type)}}.dump();
}

}  // namespace caw
