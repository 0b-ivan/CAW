// Whole matches through Session: frames, rights, revisions, tickets and privacy.
#include <doctest/doctest.h>

#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>

#include "TestPacks.h"
#include "session/Session.h"

using caw::Command;
using caw::ConnectionId;
using caw::Outgoing;
using caw::Session;
using nlohmann::json;

namespace {
const std::string kHostToken = "0123456789abcdef0123456789abcdef";

// Collects every frame per connection, the way the WebSocket transport would deliver them.
struct Table {
  Session session{kHostToken, testPacks(), kTestPacks, 42, 1000};
  std::map<ConnectionId, std::vector<json>> inbox;
  std::map<ConnectionId, bool> closed;
  std::map<ConnectionId, std::string> tickets;
  std::uint32_t now = 1000;

  void deliver() {
    for (const Outgoing& frame : session.takeOutgoing()) {
      inbox[frame.connection].push_back(json::parse(frame.text));
      if (frame.close) closed[frame.connection] = true;
      const json& last = inbox[frame.connection].back();
      if (last.at("type") == "welcome") tickets[frame.connection] = last.at("token");
    }
  }
  void send(ConnectionId connection, const Command& command) {
    session.handle(connection, command, now);
    deliver();
  }
  const json& last(ConnectionId connection) { return inbox[connection].back(); }
  json lastState(ConnectionId connection) {
    for (auto frame = inbox[connection].rbegin(); frame != inbox[connection].rend(); ++frame)
      if (frame->at("type") == "state") return *frame;
    return json();
  }
  void join(ConnectionId connection, const std::string& name, const std::string& player = "") {
    Command command;
    command.type = Command::Type::Join;
    command.name = name;
    command.player = player;
    send(connection, command);
  }
  void host(ConnectionId connection, Command::Type type, std::uint8_t index = 0) {
    Command command;
    command.type = type;
    command.index = index;
    send(connection, command);
  }
  void claimHost(ConnectionId connection, const std::string& token = kHostToken) {
    Command command;
    command.type = Command::Type::Host;
    command.token = token;
    send(connection, command);
  }
  // Submits the first `pick` hand cards with the revision this connection last saw.
  void submit(ConnectionId connection) {
    const json state = lastState(connection);
    Command command;
    command.type = Command::Type::Submit;
    command.token = tickets[connection];
    command.revision = state.at("me").at("revision");
    for (std::uint8_t slot = 0; slot < state.at("pick").get<int>(); ++slot)
      command.slots.push_back(slot);
    send(connection, command);
  }
  void judge(ConnectionId connection, std::uint8_t index) {
    Command command;
    command.type = Command::Type::Judge;
    command.token = tickets[connection];
    command.revision = lastState(connection).at("me").at("revision");
    command.index = index;
    send(connection, command);
  }
  void playerCommand(ConnectionId connection, Command::Type type) {
    Command command;
    command.type = type;
    command.token = tickets[connection];
    send(connection, command);
  }
  // Three players on connections 1-3 and a host-only connection 9, match started.
  void startThree() {
    join(1, "Ada");
    join(2, "Bert");
    join(3, "Cyra");
    claimHost(9);
    host(9, Command::Type::Start);
  }
  ConnectionId juror() {
    for (ConnectionId c : {1u, 2u, 3u})
      if (lastState(c).at("me").at("isJuror") == true) return c;
    return 0;
  }
};
}  // namespace

TEST_CASE("joining gives a ticket, a seat and a hand only to the joining connection") {
  Table table;
  table.join(1, "Ada");
  table.join(2, "Bert");
  CHECK(table.inbox[1].front().at("type") == "welcome");
  CHECK(table.tickets[1].size() == 32);
  CHECK(table.tickets[1] != table.tickets[2]);
  for (ConnectionId c : {1u, 2u})
    for (const json& frame : table.inbox[c]) {
      if (frame.at("type") != "state") continue;
      // A connection only ever sees its own seat in "me".
      CHECK(frame.at("me").at("seat") == table.inbox[c].front().at("seat"));
    }
  CHECK(table.lastState(1).at("players").size() == 2);
}

TEST_CASE("host commands need the host token") {
  Table table;
  table.join(1, "Ada");
  table.join(2, "Bert");
  table.join(3, "Cyra");
  table.host(1, Command::Type::Start);
  CHECK(table.last(1).at("code") == "not_host");
  table.claimHost(9, "ffffffffffffffffffffffffffffffff");
  CHECK(table.last(9).at("code") == "not_host");
  table.claimHost(9);
  // The notice, then the table right away, before anything changes.
  REQUIRE(table.inbox[9].size() >= 2);
  CHECK(table.inbox[9][table.inbox[9].size() - 2].at("type") == "host");
  CHECK(table.last(9).at("type") == "state");
  CHECK(table.last(9).at("players").size() == 3);
  table.host(9, Command::Type::Start);
  CHECK(table.lastState(1).at("phase") == "playing");
  CHECK(table.lastState(9).at("me").at("isHost") == true);
  CHECK(table.lastState(9).at("me").contains("hand") == false);
}

TEST_CASE("a full round: answers, anonymous judging, reveal with author and points") {
  Table table;
  table.join(1, "Ada");
  table.join(2, "Bert");
  table.join(3, "Cyra");
  table.claimHost(9);
  // With a different hand size the shuffle can put Rando first. This test
  // checks the human winner, so disable Rando rather than depend on RNG order.
  Command noRando;
  noRando.type = Command::Type::Settings;
  noRando.settings.rando = false;
  table.send(9, noRando);
  table.host(9, Command::Type::Start);
  const ConnectionId juror = table.juror();
  REQUIRE(juror != 0);
  for (ConnectionId c : {1u, 2u, 3u})
    if (c != juror) table.submit(c);
  json state = table.lastState(juror);
  REQUIRE(state.at("phase") == "judging");
  REQUIRE(state.at("plays").size() == 2);  // only the two human answers
  for (const json& play : state.at("plays")) CHECK(play.contains("author") == false);
  table.judge(juror, 0);
  state = table.lastState(1);
  CHECK(state.at("phase") == "reveal");
  CHECK(state.at("winner") != "");
  CHECK(state.at("plays").at(state.at("selected").get<int>()).at("author") == state.at("winner"));
  int points = 0;
  for (const json& player : state.at("players")) points += player.at("points").get<int>();
  CHECK(points == 1);
}

TEST_CASE("a big screen follows the table without ever seeing a hand") {
  Table table;
  table.session.addScreen(50, table.now);
  table.deliver();
  REQUIRE(table.inbox[50].size() == 1);
  CHECK(table.inbox[50].back().at("type") == "screen");
  CHECK(table.inbox[50].back().at("phase") == "lobby");

  table.startThree();
  const ConnectionId juror = table.juror();
  for (ConnectionId c : {1u, 2u, 3u})
    if (c != juror) table.submit(c);
  const json screen = table.inbox[50].back();
  CHECK(screen.at("phase") == "judging");
  CHECK(screen.at("plays").size() == 3);
  // Every screen frame is public: no "me", no hand, no tickets.
  for (const json& frame : table.inbox[50]) {
    CHECK(frame.at("type") == "screen");
    CHECK(frame.contains("me") == false);
    CHECK(frame.dump().find("hand") == std::string::npos);
  }

  table.session.connectionClosed(50, table.now);
  const std::size_t before = table.inbox[50].size();
  table.judge(juror, 0);
  CHECK(table.inbox[50].size() == before);  // a closed screen gets nothing more
}

TEST_CASE("a stale revision is rejected and the sender gets the current state again") {
  Table table;
  table.startThree();
  ConnectionId answerer = table.juror() == 1 ? 2 : 1;
  table.submit(answerer);
  const std::size_t frames = table.inbox[answerer].size();
  table.submit(answerer);  // same revision again: a duplicate delivery
  REQUIRE(table.inbox[answerer].size() == frames + 2);
  CHECK(table.inbox[answerer][frames].at("code") == "rejected");
  CHECK(table.inbox[answerer][frames + 1].at("type") == "state");
}

TEST_CASE("resume moves the seat to the new socket and closes the old one") {
  Table table;
  table.join(1, "Ada");
  const std::string oldTicket = table.tickets[1];
  Command resume;
  resume.type = Command::Type::Resume;
  resume.token = oldTicket;
  table.send(5, resume);
  CHECK(table.inbox[5].front().at("type") == "welcome");
  CHECK(table.tickets[5] != oldTicket);
  CHECK(table.last(1).at("type") == "replaced");
  CHECK(table.closed[1]);
  // The old ticket is spent.
  table.send(6, resume);
  CHECK(table.last(6).at("type") == "expired");
}

TEST_CASE("joining a running game is refused with a reason") {
  Table table;
  table.startThree();
  table.join(4, "Dora");
  CHECK(table.last(4).at("code") == "game_running");
}

TEST_CASE("kick tells the phone, closes its socket and frees the seat") {
  Table table;
  table.join(1, "Ada");
  table.join(2, "Bert");
  table.claimHost(9);
  const std::uint8_t seat = table.inbox[2].front().at("seat");
  table.host(9, Command::Type::Kick, seat);
  CHECK(table.last(2).at("type") == "kicked");
  CHECK(table.closed[2]);
  CHECK(table.lastState(1).at("players").size() == 1);
}

TEST_CASE("only the pauser or the host lifts a pause") {
  Table table;
  table.startThree();
  table.playerCommand(1, Command::Type::Pause);
  CHECK(table.lastState(2).at("pausedBy") == "Ada");
  table.playerCommand(2, Command::Type::Unpause);
  CHECK(table.last(2).at("type") == "state");  // the resync after the rejection
  CHECK(table.lastState(2).at("pausedBy") == "Ada");
  table.host(9, Command::Type::HostUnpause);
  CHECK(table.lastState(2).at("pausedBy") == "");
}

TEST_CASE("a closed socket marks the seat offline without freeing it") {
  Table table;
  table.join(1, "Ada");
  table.join(2, "Bert");
  table.session.connectionClosed(2, table.now);
  table.deliver();
  const json players = table.lastState(1).at("players");
  REQUIRE(players.size() == 2);
  CHECK(players.at(1).at("connected") == false);
}

TEST_CASE("a saved match comes back after a restart and goes on with the same tickets") {
  Table before;
  before.startThree();
  const ConnectionId juror = before.juror();
  ConnectionId answerer = juror == 1 ? 2 : 1;
  before.submit(answerer);
  const std::string saved = before.session.saveState(before.now);

  // A new process: same cards, fresh session, nobody connected yet.
  Table after;
  REQUIRE(after.session.loadState(saved, after.now));
  CHECK(after.session.takeChanged());
  after.deliver();
  CHECK(after.inbox.empty());

  // Phones reconnect on new sockets with the tickets they kept.
  for (ConnectionId c : {1u, 2u, 3u}) {
    Command resume;
    resume.type = Command::Type::Resume;
    resume.token = before.tickets[c];
    after.send(c + 20, resume);
    REQUIRE(after.inbox[c + 20].front().at("type") == "welcome");
  }
  json state = after.lastState(answerer + 20);
  CHECK(state.at("phase") == "playing");
  CHECK(state.at("me").at("submitted") == true);
  CHECK(state.at("submitted") == 1);
  for (const json& player : state.at("players")) CHECK(player.at("connected") == true);

  // The host claims the room again; the match continues to the reveal.
  after.claimHost(29);
  for (ConnectionId c : {1u, 2u, 3u})
    if (c != juror && c != answerer) after.submit(c + 20);
  after.judge(juror + 20, 0);
  CHECK(after.lastState(21).at("phase") == "reveal");
}

TEST_CASE("a snapshot that does not fit is refused and changes nothing") {
  Table table;
  table.join(1, "Ada");
  CHECK_FALSE(table.session.loadState("{}", table.now));
  CHECK_FALSE(table.session.loadState("not json", table.now));
  table.deliver();
  CHECK(table.session.seatedCount() == 1);
}

namespace {
Command settings(std::uint8_t rounds, std::uint8_t answerSecs) {
  Command command;
  command.type = Command::Type::Settings;
  command.settings.rounds = rounds;
  command.settings.answerSecs = answerSecs;
  command.settings.extensionSecs = 0;
  return command;
}
}  // namespace

TEST_CASE("new sessions deal seven cards and the host can change the hand only in the lobby") {
  Table table;
  table.join(1, "Ada");
  table.join(2, "Bert");
  table.join(3, "Cyra");
  table.claimHost(9);
  CHECK(table.lastState(1).at("settings").at("hand") == 7);

  Command change = settings(10, 0);
  change.settings.hand = 5;
  table.send(9, change);
  CHECK(table.lastState(1).at("settings").at("hand") == 5);

  table.host(9, Command::Type::Start);
  CHECK(table.lastState(1).at("me").at("hand").size() == 5);
  const std::string before = table.session.saveState(table.now);
  change.settings.hand = 8;
  table.send(9, change);
  CHECK(table.last(9).at("code") == "rejected");
  CHECK(table.session.saveState(table.now) == before);
  CHECK(table.lastState(1).at("settings").at("hand") == 5);

  // The host may still change clocks during the match.
  change.settings.hand = 5;
  change.settings.answerSecs = 30;
  table.send(9, change);
  CHECK(table.lastState(1).at("settings").at("answerSecs") == 30);
}

TEST_CASE("new sessions start with the configured default seven-card hand") {
  Table table;
  table.startThree();
  const json state = table.lastState(1);
  CHECK(state.at("settings").at("hand") == 7);
  CHECK(state.at("me").at("hand").size() == 7);
}

TEST_CASE("the host sets rounds in the lobby and clocks at any time") {
  Table table;
  table.join(1, "Ada");
  table.join(2, "Bert");
  table.join(3, "Cyra");
  table.claimHost(9);
  table.send(9, settings(5, 0));
  CHECK(table.lastState(1).at("settings").at("rounds") == 5);

  table.host(9, Command::Type::Start);
  // A running match keeps its number of rounds, but takes a new answer clock.
  table.send(9, settings(8, 30));
  CHECK(table.last(9).at("code") == "rejected");
  table.send(9, settings(5, 30));
  CHECK(table.lastState(1).at("settings").at("answerSecs") == 30);

  // The clock starts with the next round and closes it when it runs out.
  table.host(9, Command::Type::Abort);
  table.host(9, Command::Type::Next);
  json state = table.lastState(1);
  REQUIRE(state.at("phase") == "playing");
  CHECK(state.at("clock") == "answers");
  CHECK(state.at("left") == 30000);
  table.session.tick(table.now + 31000);
  table.deliver();
  CHECK(table.lastState(1).at("phase") != "playing");
}

TEST_CASE("the host switches packs in the lobby only") {
  caw::PackLibrary& packs = testPacks();
  if (packs.find("tiny") == nullptr) {
    caw::CardPack tiny{"tiny", "Klein", "de", "CC0", "Test", "https://example.org", {}, {}};
    tiny.prompts.push_back({"Kleine Frage ___.", 1, 0});
    for (int i = 0; i < 100; ++i) tiny.answers.push_back("Klein " + std::to_string(i));
    packs.add(tiny);
  }
  Table table;
  table.claimHost(9);
  CHECK(table.lastState(9).at("packs").at(0).at("id") == "test");
  Command command;
  command.type = Command::Type::Packs;
  command.packs = {"tiny"};
  table.send(9, command);
  json state = table.lastState(9);
  REQUIRE(state.at("packs").size() == 1);
  CHECK(state.at("packs").at(0).at("id") == "tiny");
  CHECK(state.at("packs").at(0).at("license") == "CC0");
  CHECK(table.session.packs() == std::vector<std::string>{"tiny"});

  command.packs = {"missing"};
  table.send(9, command);
  CHECK(table.last(9).at("code") == "rejected");

  table.startThree();
  CHECK(table.lastState(1).at("prompt") == "Kleine Frage ___.");
  command.packs = {"test"};
  table.send(9, command);
  CHECK(table.last(9).at("code") == "rejected");  // the match runs: cards stay
  CHECK(table.session.packs() == std::vector<std::string>{"tiny"});
}

TEST_CASE("every seat gets a word nobody else at the table has") {
  Table table;
  // The same device id twice (two tabs of one phone) must not share an identity.
  table.join(1, "Ada", "00000000000000aa");
  table.join(2, "Ada", "00000000000000aa");
  table.join(3, "Bert");
  const json players = table.lastState(1).at("players");
  REQUIRE(players.size() == 3);
  std::set<std::string> words;
  for (const json& player : players) {
    CHECK(player.at("word") != "");
    words.insert(player.at("word").get<std::string>());
    CHECK(player.contains("player") == false);  // ids never leave the server
  }
  CHECK(words.size() == 3);
  const auto identities = table.session.identities();
  CHECK(identities[0].playerId == "00000000000000aa");
  CHECK(identities[1].playerId != "00000000000000aa");
  CHECK(identities[2].playerId.size() == 16);  // a player without an id gets one
}

TEST_CASE("rounds and matches become history, with the winning sentences of the match") {
  Table table;
  table.join(1, "Ada", "00000000000000aa");
  table.join(2, "Bert", "00000000000000bb");
  table.join(3, "Cyra", "00000000000000cc");
  table.claimHost(9);
  Command rounds;
  rounds.type = Command::Type::Settings;
  rounds.settings.rounds = 3;
  rounds.settings.rando = false;
  table.send(9, rounds);
  table.host(9, Command::Type::Start);
  for (int round = 0; round < 3; ++round) {
    const ConnectionId juror = table.juror();
    for (ConnectionId c : {1u, 2u, 3u})
      if (c != juror) table.submit(c);
    table.judge(juror, 0);
    table.host(9, Command::Type::Next);
  }
  const json final = table.lastState(1);
  REQUIRE(final.at("phase") == "finished");
  REQUIRE(final.at("lines").size() == 3);
  CHECK(final.at("lines").at(0).at("sentence").get<std::string>().find("Testfrage") == 0);

  const auto events = table.session.takeHistory();
  int roundsWon = 0, matches = 0, winners = 0;
  for (const auto& event : events) {
    if (event.kind == caw::HistoryEvent::Kind::RoundWon) {
      ++roundsWon;
      CHECK(event.sentence.find("___") == std::string::npos);  // the blanks are filled
    } else {
      ++matches;
      if (event.wonMatch) ++winners;
    }
  }
  CHECK(roundsWon == 3);
  CHECK(matches == 3);
  CHECK(winners >= 1);
  CHECK(table.session.takeHistory().empty());  // counted once

  // A rematch starts a new list of sentences.
  table.host(9, Command::Type::Rematch);
  table.host(9, Command::Type::Start);
  CHECK(table.lastState(1).at("lines").empty());
}

TEST_CASE("a locked lobby turns new players away, seated ones keep playing") {
  Table table;
  table.join(1, "Ada");
  table.claimHost(9);
  table.host(9, Command::Type::Lock);
  CHECK(table.lastState(1).at("locked") == true);
  table.join(2, "Bert");
  CHECK(table.last(2).at("code") == "locked");
  table.host(9, Command::Type::Lock);
  CHECK(table.last(9).at("code") == "rejected");  // already locked
  table.host(9, Command::Type::Unlock);
  table.join(2, "Bert");
  CHECK(table.last(2).at("type") != "error");
  CHECK(table.lastState(1).at("players").size() == 2);
}

TEST_CASE("settings from a player are refused") {
  Table table;
  table.join(1, "Ada");
  table.send(1, settings(5, 0));
  CHECK(table.last(1).at("code") == "not_host");
}
