// A snapshot must preserve future actions, not just the public scoreboard.
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "CardPartyDemoDeck.h"
#include "net/Messages.h"

using namespace cardparty;
using caw::Messages;
namespace {
struct Match {
  Game game;
  Table table{game};
  Sessions seats{game};
  SessionTicket tickets[kPlayers]{};
  explicit Match(unsigned players = 3, std::uint32_t seed = 7) {
    REQUIRE(game.configure(demoDeck(), Rules{10, 10, true}, seed));
    for (unsigned i = 0; i < players; ++i) {
      std::uint8_t secret[kTicketBytes]{};
      secret[0] = i + 1;
      const std::string name = "Test player " + std::to_string(i);
      REQUIRE(seats.join(i + 10, name.c_str(), secret, tickets[i]));
    }
    Settings settings;
    settings.answerSecs = 30;
    settings.jurorSecs = 20;
    settings.autoSecs = 5;
    table.apply(settings, 1000);
    table.tick(1000);
  }
};
void roundtrip(Match& source, std::uint32_t now = 1000) {
  Match restored(3, 999);
  const std::string snapshot = Messages::saveState(source.game, source.table, source.seats, now);
  REQUIRE(snapshot.size() < 65536);
  REQUIRE(Messages::loadState(snapshot, restored.game, restored.table, restored.seats, 90000));
  CHECK(Messages::saveState(restored.game, restored.table, restored.seats, 90000) == snapshot);
  for (unsigned i = 0; i < source.game.seatedCount(); ++i) {
    if (!source.game.publicView().players[i].connected) continue;
    HandView expected, actual;
    std::uint32_t a = 0, b = 0;
    REQUIRE(source.seats.privateHand(i + 10, source.tickets[i].secret, expected, a));
    REQUIRE(restored.seats.privateHand(i + 10, source.tickets[i].secret, actual, b));
    CHECK(a == b);
    CHECK(actual.size == expected.size);
    for (unsigned j = 0; j < expected.size; ++j) CHECK(actual.cards[j] == expected.cards[j]);
  }
}
void submitAll(Game& game) {
  const auto view = game.publicView();
  const std::uint8_t slots[] = {0, 1, 2};
  for (unsigned i = 0; i < kPlayers; ++i) {
    HandView hand;
    if (i != view.czar && game.privateHand(i, hand) && !hand.submitted)
      REQUIRE(game.submit(i, slots, view.pick));
  }
}
}  // namespace

TEST_CASE("snapshots cover lobby, hands, submitted plays, clocks, pause, block and votes") {
  Match match;
  roundtrip(match);
  REQUIRE(match.seats.hostStart());
  match.table.tick(1000);
  roundtrip(match);
  const auto view = match.game.publicView();
  const unsigned player = (view.czar + 1) % 3;
  REQUIRE(match.seats.swap(player + 10, match.tickets[player].secret, 0, 0));
  roundtrip(match);
  REQUIRE(match.table.pause(kHost, 2000));
  roundtrip(match, 2000);
  REQUIRE(match.table.unpause(kHost, 3000));
  const std::uint8_t slots[] = {0, 1, 2};
  REQUIRE(match.seats.submit(player + 10, match.tickets[player].secret, 1, slots, view.pick));
  roundtrip(match, 3000);
  const unsigned missing = (view.czar + 2) % 3;
  REQUIRE(match.seats.disconnect(missing + 10));
  match.table.tick(3000);
  roundtrip(match, 4000);
  match.table.tick(18000);
  REQUIRE(match.table.blockedOn() == missing);
  roundtrip(match, 18000);
  REQUIRE(match.table.continueWithout(18000));
  REQUIRE(match.seats.hostAbort());
  match.table.tick(18000);
  roundtrip(match, 18000);
  REQUIRE(match.game.endGame());
  REQUIRE(match.table.vote(1));
  match.table.endedByHost();
  match.table.tick(19000);
  roundtrip(match, 19000);
  REQUIRE(match.game.rematch());
  match.table.tick(20000);
  roundtrip(match, 20000);
}

TEST_CASE("restored random state and draw piles produce the same entire match") {
  for (unsigned players : {3u, 8u}) {
    Match original(players, 89), restored(players, 1);
    REQUIRE(original.seats.hostStart());
    original.table.tick(1000);
    const auto snapshot = Messages::saveState(original.game, original.table, original.seats, 1000);
    REQUIRE(Messages::loadState(snapshot, restored.game, restored.table, restored.seats, 1000));
    while (original.game.phase() != Phase::Finished) {
      submitAll(original.game);
      submitAll(restored.game);
      original.table.tick(1000);
      restored.table.tick(1000);
      CHECK(Messages::saveState(original.game, original.table, original.seats, 1000) ==
            Messages::saveState(restored.game, restored.table, restored.seats, 1000));
      roundtrip(original);
      const auto juror = original.game.publicView().czar;
      REQUIRE(original.game.judge(juror, 0));
      REQUIRE(restored.game.judge(juror, 0));
      original.table.tick(1000);
      restored.table.tick(1000);
      roundtrip(original);
      REQUIRE(original.game.nextRound());
      REQUIRE(restored.game.nextRound());
      original.table.tick(1000);
      restored.table.tick(1000);
      CHECK(Messages::saveState(original.game, original.table, original.seats, 1000) ==
            Messages::saveState(restored.game, restored.table, restored.seats, 1000));
    }
    roundtrip(original);
  }
}

TEST_CASE("snapshots reject malformed data atomically") {
  Match match;
  REQUIRE(match.seats.hostStart());
  match.table.tick(1000);
  const auto saved = Messages::saveState(match.game, match.table, match.seats, 1000);
  const auto reject = [&](std::string text) {
    CHECK_FALSE(Messages::loadState(text, match.game, match.table, match.seats, 1000));
    CHECK(Messages::saveState(match.game, match.table, match.seats, 1000) == saved);
  };
  reject("{");
  reject("[]");
  reject(std::string(65537, ' '));
  for (const auto& patch :
       {nlohmann::json{{"schema", 2}}, nlohmann::json{{"deck", 0}},
        nlohmann::json{{"game", {{"rng", 0}}}}, nlohmann::json{{"game", {{"phase", 9}}}},
        nlohmann::json{{"game", {{"pick", -1}}}}, nlohmann::json{{"game", {{"count", 8}}}},
        nlohmann::json{{"game", {{"players", nlohmann::json::array()}}}},
        nlohmann::json{{"table", {{"remaining", UINT32_MAX}}}},
        nlohmann::json{{"seats", nlohmann::json::array()}}}) {
    auto corrupted = nlohmann::json::parse(saved);
    corrupted.merge_patch(patch);
    reject(corrupted.dump());
  }
  auto corrupted = nlohmann::json::parse(saved);
  corrupted["game"]["players"][1]["size"] = 13;
  reject(corrupted.dump());
  corrupted = nlohmann::json::parse(saved);
  corrupted["seats"][1]["secret"] = corrupted["seats"][0]["secret"];
  reject(corrupted.dump());
  Game empty;
  Table table(empty);
  Sessions seats(empty);
  CHECK_FALSE(Messages::loadState(saved, empty, table, seats, 1000));
  CHECK_THROWS(Messages::saveState(empty, table, seats, 1000));
}

TEST_CASE("remaining clock time survives a new monotonic epoch and uint32 wrap") {
  Match match;
  REQUIRE(match.seats.hostStart());
  match.table.tick(UINT32_MAX - 1000);
  const auto saved = Messages::saveState(match.game, match.table, match.seats, UINT32_MAX - 500);
  Match restored;
  REQUIRE(Messages::loadState(saved, restored.game, restored.table, restored.seats, 5000));
  CHECK(restored.table.left(5000) == match.table.left(UINT32_MAX - 500));
  CHECK_FALSE(restored.table.extended());
  restored.table.tick(5000 + restored.table.left(5000));
  CHECK(restored.table.extended());
}

TEST_CASE("same-size decks with changed categories cannot accept a snapshot") {
  Match match;
  const auto saved = Messages::saveState(match.game, match.table, match.seats, 1000);
  Deck changed = demoDeck();
  std::uint16_t masks[12] = {1};
  changed.promptMasks = masks;
  Game game;
  REQUIRE(game.configure(changed, Rules{}, 1));
  Table table(game);
  Sessions seats(game);
  CHECK_FALSE(Messages::loadState(saved, game, table, seats, 1000));
  CHECK(game.phase() == Phase::Lobby);
  CHECK(game.seatedCount() == 0);
}
