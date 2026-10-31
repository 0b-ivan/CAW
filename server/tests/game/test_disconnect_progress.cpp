// Firmware regression scenarios, adapted only to the doctest harness.
#include <doctest/doctest.h>
// CardParty lost-connection regressions. A submitted offline player must
// not cause the remaining active submitters to lose their turn.
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include "CardPartyCore.h"
#include "CardPartySessions.h"
using namespace cardparty;

namespace {
struct Fixture {
  char names[110][24] = {};
  Answer answers[110] = {};
  Prompt prompts[3] = {
    {"Whose fault is ____?", 1, 0},
    {"____ and ____", 2, 0},
    {"____, ____ and ____", 3, 2}
  };
  Deck deck{};
  Fixture() {
    for(int i=0;i<110;++i) {
      std::snprintf(names[i],sizeof names[i],"Card %d",i);
      answers[i].text=names[i];
    }
    deck=Deck{prompts,3,answers,110};
  }
};
struct Key {
  uint8_t bytes[kTicketBytes]={};
  explicit Key(uint8_t base) {
    for(size_t i=0;i<kTicketBytes;++i) bytes[i]=static_cast<uint8_t>(base+i+1);
  }
};
void submittedPlayerDisconnectMustNotSkipOtherPlayer() {
  Fixture f;
  Game g;
  REQUIRE((g.configure(f.deck,Rules{5,3,true},0x137acafe)));
  Sessions sessions(g);
  const Key a(10), b(40), c(70), fresh(100), rogue(160);
  SessionTicket ta{},tb{},tc{},renewed{};
  REQUIRE((sessions.join(10,"Judge",a.bytes,ta)));
  REQUIRE((sessions.join(11,"Alice",b.bytes,tb)));
  REQUIRE((sessions.join(12,"Bob",c.bytes,tc)));
  REQUIRE((sessions.hostStart()));
  const auto initial=g.publicView();
  REQUIRE((initial.phase==Phase::Playing && initial.expected==2 && initial.czar==ta.seat));

  const uint8_t slots[]={0,1,2};
  REQUIRE((sessions.submit(11,b.bytes,0,slots,initial.pick)));
  REQUIRE((g.publicView().submitted==1));
  // Before the fix, this disconnect decremented expected to 1 and
  // promoted phase to Judging although Bob was still selecting his hand.
  REQUIRE((sessions.disconnect(11)));
  const auto interrupted=g.publicView();
  REQUIRE((interrupted.phase==Phase::Playing));
  REQUIRE((interrupted.submitted==1));
  REQUIRE((interrupted.expected==2));
  REQUIRE((interrupted.players[tb.seat].connected==false));
  REQUIRE((!sessions.submit(11,b.bytes,1,slots,initial.pick)));
  REQUIRE((sessions.submit(12,c.bytes,0,slots,initial.pick)));
  const auto judging=g.publicView();
  REQUIRE((judging.phase==Phase::Judging));
  REQUIRE((judging.submitted==2 && judging.expected==2));
  REQUIRE((judging.plays==3)); // Alice + Bob + Rando
  for(uint8_t i=0;i<judging.plays;++i)
    REQUIRE((judging.pile[i].author==kNone));

  // A disconnected Card Czar may not choose a winner from a stale socket
  // or the old token. Resume preserves the seat but rotates the secret.
  REQUIRE((sessions.disconnect(10)));
  REQUIRE((!sessions.judge(10,a.bytes,0,0)));
  REQUIRE((!sessions.resume(13,rogue.bytes,fresh.bytes,renewed)));
  REQUIRE((sessions.resume(13,a.bytes,fresh.bytes,renewed)));
  REQUIRE((renewed.seat==ta.seat));
  REQUIRE((!sessions.judge(13,a.bytes,0,0)));
  REQUIRE((sessions.judge(13,fresh.bytes,0,0)));
  REQUIRE((g.publicView().phase==Phase::Reveal));
  REQUIRE((!sessions.judge(13,fresh.bytes,0,0)));
  // Since only two human seats are online, nextRound must be rejected
  // until the previously submitted player returns.
  REQUIRE((!sessions.hostNext()));
  const Key freshB(130);
  SessionTicket resumedB{};
  REQUIRE((sessions.resume(14,b.bytes,freshB.bytes,resumedB)));
  REQUIRE((sessions.hostNext()));
  REQUIRE((g.publicView().phase==Phase::Playing));
  REQUIRE((g.publicView().round==2));
}
void unsubmittedDisconnectMayFinishRound() {
  Fixture f;
  Game g;
  REQUIRE((g.configure(f.deck,Rules{5,2,true},0x1011)));
  Sessions s(g);
  const Key a(1),b(22),c(43);
  SessionTicket ta{},tb{},tc{};
  REQUIRE((s.join(0,"Judge",a.bytes,ta)));
  REQUIRE((s.join(1,"Alice",b.bytes,tb)));
  REQUIRE((s.join(2,"Bob",c.bytes,tc)));
  REQUIRE((s.hostStart()));
  // Bob is disconnected BEFORE submitting. His turn is held (screen lock,
  // app switch) so Alice's answer alone does not skip him...
  REQUIRE((s.disconnect(2)));
  const auto v=g.publicView();
  REQUIRE((v.phase==Phase::Playing && v.expected==2 && g.held(tc.seat)));
  const uint8_t slots[]={0,1,2};
  REQUIRE((s.submit(1,b.bytes,0,slots,v.pick)));
  REQUIRE((g.publicView().phase==Phase::Playing));
  // ...until the transport's grace window releases it; then the round
  // advances instead of hanging forever on a player who disappeared.
  REQUIRE((g.releaseTurn(tc.seat)));
  REQUIRE((!g.releaseTurn(tc.seat)));
  const auto judging=g.publicView();
  REQUIRE((judging.phase==Phase::Judging && judging.expected==1));
  REQUIRE((judging.submitted==1 && judging.plays==2));
  REQUIRE((s.judge(0,a.bytes,0,0)));
  REQUIRE((g.publicView().phase==Phase::Reveal));
  // Finish can be recovered after returning offline player.
  REQUIRE((!s.hostNext()));
  const Key fresh(80);
  SessionTicket resume{};
  REQUIRE((s.resume(3,c.bytes,fresh.bytes,resume)));
  REQUIRE((s.hostNext()));
}
void multipleSubmittedDisconnectsStillWaitForRemainingSeats() {
  Fixture f;
  Game g;
  REQUIRE((g.configure(f.deck,Rules{5,1,true},0x303)));
  for(int i=0;i<8;++i) {
    char name[16];std::snprintf(name,sizeof name,"Player %d",i);
    REQUIRE((g.join(name)==i));
  }
  REQUIRE((g.start()));
  const auto start=g.publicView();
  const uint8_t slots[]={0,1,2};
  // Six players have answered; all but the still-pending seventh disconnect.
  for(uint8_t i=1;i<7;++i) {
    REQUIRE((g.submit(i,slots,start.pick)));
    REQUIRE((g.connected(i,false)));
    const auto v=g.publicView();
    REQUIRE((v.phase==Phase::Playing));
    REQUIRE((v.submitted==i));
    REQUIRE((v.expected==7));
  }
  REQUIRE((g.submit(7,slots,start.pick)));
  const auto v=g.publicView();
  REQUIRE((v.phase==Phase::Judging));
  REQUIRE((v.expected==7 && v.submitted==7 && v.plays==8));
  REQUIRE((g.judge(0,0)));
  // One round requested, eight at the table: a full Juror cycle is played.
  REQUIRE((g.rules().rounds==8 && g.phase()==Phase::Reveal));
  // Six answerers are still offline: no next round until three are back.
  REQUIRE((!g.nextRound()));
}
void heldTurnReturnsBeforeGrace() {
  Fixture f;
  Game g;
  REQUIRE((g.configure(f.deck,Rules{5,2,true},0x2024)));
  for(const char* n:{"Czar","Alice","Bob"}) REQUIRE((g.join(n)!=kNone));
  REQUIRE((g.start()));
  const auto v=g.publicView();
  const uint8_t alice=static_cast<uint8_t>((v.czar+1)%3), bob=static_cast<uint8_t>((v.czar+2)%3);
  const uint8_t slots[]={0,1,2};
  REQUIRE((g.submit(alice,slots,v.pick)));
  // Bob locks his phone: held, round waits.
  REQUIRE((g.connected(bob,false) && g.held(bob)));
  REQUIRE((g.phase()==Phase::Playing && g.publicView().expected==2));
  // He returns within the grace window and answers normally.
  REQUIRE((g.connected(bob,true) && !g.held(bob)));
  REQUIRE((g.submit(bob,slots,v.pick)));
  REQUIRE((g.phase()==Phase::Judging && g.publicView().plays==3));
  // The Czar and players who already answered are never held.
  REQUIRE((g.connected(alice,false) && !g.held(alice)));
  REQUIRE((g.connected(v.czar,false) && !g.held(v.czar)));
  // The host may still close a stalled round at any time.
  Game h;
  REQUIRE((h.configure(f.deck,Rules{5,2,true},0x2025)));
  for(const char* n:{"Czar","Alice","Bob"}) REQUIRE((h.join(n)!=kNone));
  REQUIRE((h.start()));
  const auto w=h.publicView();
  const uint8_t a2=static_cast<uint8_t>((w.czar+1)%3), b2=static_cast<uint8_t>((w.czar+2)%3);
  REQUIRE((h.submit(a2,slots,w.pick)));
  REQUIRE((h.connected(b2,false) && h.held(b2)));
  REQUIRE((h.closeSubmissions() && h.phase()==Phase::Judging));
}
} // namespace
TEST_CASE("upstream test_disconnect_progress") {
  submittedPlayerDisconnectMustNotSkipOtherPlayer();
  unsubmittedDisconnectMayFinishRound();
  multipleSubmittedDisconnectsStillWaitForRemainingSeats();
  heldTurnReturnsBeforeGrace();
  std::puts("cardparty disconnect regression: submitted/unsent/held-turn/Czar/8-seat transitions PASS");
}
