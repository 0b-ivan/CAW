// The host is a role independent of the eight player seats.
#include <doctest/doctest.h>

#include "CardPartyDemoDeck.h"
#include "CardPartyTable.h"

TEST_CASE("non-playing web host can release a player pause") {
  cardparty::Game game;
  REQUIRE(game.configure(cardparty::demoDeck(), cardparty::Rules{}, 7));
  game.join("Test player 1");
  game.join("Test player 2");
  game.join("Test player 3");
  REQUIRE(game.start());
  cardparty::Table table(game);
  table.tick(100);
  REQUIRE(table.pause(1, 100));
  CHECK_FALSE(table.unpause(2, 200));
  CHECK(table.unpause(cardparty::kHost, 200));
  REQUIRE(table.pause(cardparty::kHost, 300));
  CHECK(table.pausedBy() == cardparty::kHost);
  CHECK_FALSE(table.unpause(0, 400));
  CHECK(table.unpause(cardparty::kHost, 400));
  CHECK(game.seatedCount() == 3);
  CHECK(cardparty::kHost >= cardparty::kPlayers);
}
