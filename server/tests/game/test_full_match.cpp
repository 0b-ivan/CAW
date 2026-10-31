// Firmware regression scenarios, adapted only to the doctest harness.
#include <doctest/doctest.h>
// Full offline match regression against the synthetic fixture matching upstream deck dimensions.
// The native host simulates users, not real Wi-Fi / devices.
#include <cstdio>
#include <initializer_list>
#include "CardPartyCore.h"
#include "CardPartyDemoDeck.h"
using namespace cardparty;

void runMatch(uint8_t count,uint32_t seed) {
  Game game;
  const auto& deck=demoDeck();
  REQUIRE((game.configure(deck, Rules{5,10,true},seed)));
  uint8_t seats[kPlayers]={};
  for(uint8_t i=0;i<count;++i) {
    char label[16];std::snprintf(label,sizeof(label),"Tester-%d",i);
    seats[i]=game.join(label);
    REQUIRE((seats[i]==i));
  }
  REQUIRE((game.start()));
  int humansWon=0,botWon=0;
  int pick3=0,pick2=0,draw2=0;
  // Ten requested; played as the nearest multiple of the table (9 for 3, 8 for 8).
  const uint8_t rounds=game.rules().rounds;
  REQUIRE((rounds%count==0 && rounds==Game::fairRounds(10,count)));
  int judged[kPlayers]={};
  for(uint8_t round=1;round<=rounds;++round) {
    const PublicView initial=game.publicView();
    REQUIRE((initial.phase==Phase::Playing));
    REQUIRE((initial.round==round));
    REQUIRE((initial.czar==seats[(round-1)%count]));
    REQUIRE((initial.plays==0));
    REQUIRE((initial.expected==count-1));
    if(initial.pick==2)++pick2;
    if(initial.pick==3)++pick3;
    if(initial.draw==2)++draw2;
    uint8_t slots[3]={0,1,2};
    for(uint8_t i=0;i<count;++i) {
      HandView hand{};
      REQUIRE((game.privateHand(i,hand)));
      REQUIRE((hand.size<=kHandMax));
      if(i==initial.czar)continue;
      REQUIRE((hand.size>=initial.pick));
      REQUIRE((game.submit(i,slots,initial.pick)));
      REQUIRE((!game.submit(i,slots,initial.pick)));
      const auto mid=game.publicView();
      if(mid.phase==Phase::Playing) REQUIRE((mid.plays==0));
    }
    const auto judging=game.publicView();
    REQUIRE((judging.phase==Phase::Judging));
    REQUIRE((judging.plays==count)); // n-1 humans plus the anonymous bot
    REQUIRE((judging.submitted==count-1));
    for(uint8_t i=0;i<judging.plays;++i) {
      REQUIRE((judging.pile[i].author==kNone));
      for(uint8_t a=0;a<initial.pick;++a) {
        REQUIRE((judging.pile[i].cards[a]<deck.answersCount));
        for(uint8_t b=a+1;b<initial.pick;++b)
          REQUIRE((judging.pile[i].cards[a]!=judging.pile[i].cards[b]));
      }
    }
    // Always choose the first *shuffled* anonymous response.
    REQUIRE((game.judge(initial.czar,0)));
    ++judged[initial.czar];
    const auto reveal=game.publicView();
    REQUIRE((reveal.phase==Phase::Reveal));
    REQUIRE((reveal.winner==reveal.pile[reveal.selected].author));
    if(reveal.winner==kRando) ++botWon;
    else ++humansWon;
    REQUIRE((!game.judge(initial.czar,0))); // replay protection
    int points=0;
    for(uint8_t i=0;i<count;++i)points+=reveal.players[i].points;
    REQUIRE((points==humansWon));
    REQUIRE((game.nextRound()));
  }
  REQUIRE((game.phase()==Phase::Finished));
  REQUIRE((game.publicView().round==rounds));
  REQUIRE((!game.nextRound()));
  for(uint8_t i=0;i<count;++i) REQUIRE((judged[i]==rounds/count));  // everyone judged equally often
  std::printf("PASS demo %d rounds, %d humans, seed=%u, pick2=%d, pick3=%d, draw2=%d, bot wins=%d\n",
      rounds, count, seed, pick2,pick3,draw2,botWon);
}
TEST_CASE("upstream test_full_match") {
  // Three-player real-world baseline, maximum-eight-player capacity, plus
  // additional seeds to avoid relying on one predetermined card shuffle.
  for(uint8_t n: {uint8_t(3),uint8_t(8)})
    for(uint32_t seed: {uint32_t(1),uint32_t(89),uint32_t(20261027)})
      runMatch(n,seed);
  std::puts("PASS CardParty full demo-game simulator scenarios");
}
