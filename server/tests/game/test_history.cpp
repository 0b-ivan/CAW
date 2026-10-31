// Firmware regression scenarios, adapted only to the doctest harness.
#include <doctest/doctest.h>
#include <cstdint>
#include <cstring>
#include <vector>
#include "CardPartyHistory.h"

using namespace cardparty::history;

TEST_CASE("upstream test_history") {
  Book book;
  const uint64_t alice = UINT64_C(0x11112222deadbeef);
  const uint64_t bob = UINT64_C(0x33334444deadbeef);  // same truncated 32-bit ID
  const uint64_t carol = UINT64_C(0x55556666deadbeef);
  const uint64_t dave = UINT64_C(0x77778888deadbeef);
  REQUIRE((book.upsert(alice,"Alice",10)));
  REQUIRE((book.upsert(bob,"Bob",11)));
  REQUIRE((book.upsert(carol,"Carol",12)));
  REQUIRE((book.player(alice)->id != book.player(bob)->id));
  REQUIRE((book.player(alice)->word == Book::wordFor(alice)));
  REQUIRE((book.upsert(alice,"Alice",15,7)));
  REQUIRE((book.player(alice)->word == 7));
  REQUIRE((!book.upsert(0,"Invalid",13)));
  REQUIRE((!book.upsert(alice,"",13)));
  REQUIRE((book.played(alice)));
  REQUIRE((book.gameWon(alice)));
  REQUIRE((book.roundWon(alice,"Drei vergessene Backups",20)));
  REQUIRE((book.player(alice)->games == 1 && book.player(alice)->wins == 1));
  REQUIRE((book.player(alice)->rounds == 1 && book.quotes() == 1));
  REQUIRE((book.quote(0).player == alice));
  REQUIRE((std::strcmp(book.quote(0).sentence,"Drei vergessene Backups") == 0));

  constexpr uint32_t pack = 0xabcde123;
  REQUIRE((book.strike(alice,pack,4,17) == Book::Vote::Recorded));
  REQUIRE((book.strike(alice,pack,4,17) == Book::Vote::Duplicate));
  REQUIRE((book.strike(bob,pack,4,17) == Book::Vote::Recorded));
  REQUIRE((!book.blocked(pack,4,17)));
  REQUIRE((book.strike(carol,pack,4,17) == Book::Vote::Blocked));
  REQUIRE((book.blocked(pack,4,17)));
  REQUIRE((!book.blocked(pack+1,4,17)));
  REQUIRE((!book.blocked(pack,4,18)));
  REQUIRE((!book.blocked(pack,5,17)));
  REQUIRE((book.strike(dave,pack,4,17) == Book::Vote::Blocked));
  REQUIRE((book.strike(0,pack,4,17) == Book::Vote::Invalid));
  REQUIRE((book.strike(dave,pack,UINT16_MAX,17) == Book::Vote::Invalid));

  const auto saved = book.encode();
  Book restored;
  REQUIRE((restored.decode(saved.data(),saved.size())));
  REQUIRE((restored.players()==3 && restored.quotes()==1 && restored.pairs()==1));
  REQUIRE((restored.blocked(pack,4,17)));
  REQUIRE((restored.player(alice)->wins == 1));
  REQUIRE((restored.player(alice)->word == 7));
  REQUIRE((restored.strike(alice,pack,4,17) == Book::Vote::Blocked));
  auto bad = saved;
  bad[12] ^= 0x80;
  REQUIRE((!restored.decode(bad.data(),bad.size())));
  REQUIRE((restored.player(alice)->wins == 1)); // no mutation on a corrupt file
  bad = saved;
  bad[4]=2; // unsupported future version must fail closed
  REQUIRE((!restored.decode(bad.data(),bad.size())));
  REQUIRE((!restored.decode(saved.data(),saved.size()-1)));

  // A cap of 20 retained quotes, newest first, including non-ASCII.
  for (uint32_t i=0;i<24;++i) REQUIRE((book.roundWon(alice,"Über den Wolken",100+i)));
  REQUIRE((book.quotes()==20));
  REQUIRE((book.quote(0).seen==123));
  REQUIRE((book.quote(19).seen==104));

  // Bounded 64-identity cache: least recently seen record falls out.
  for (uint64_t id=100; id<165; ++id) REQUIRE((book.upsert(id,"Guest",static_cast<uint32_t>(id+100))));
  REQUIRE((book.players()==64));
  REQUIRE((book.player(alice)==nullptr));
  // Keep all 2-vote pairs when there are too many, evict 1-vote pairs first.
  Book votes;
  for (uint16_t answer=0; answer<kNonFitPairs; ++answer)
    REQUIRE((votes.strike(alice,pack,5,answer)==Book::Vote::Recorded));
  REQUIRE((votes.pairs()==kNonFitPairs));
  REQUIRE((votes.strike(bob,pack,5,0)==Book::Vote::Recorded));
  REQUIRE((votes.strike(alice,pack,7,8)==Book::Vote::Recorded));
  REQUIRE((votes.pairs()==kNonFitPairs));
  REQUIRE((votes.strike(carol,pack,5,0)==Book::Vote::Blocked));
  const auto large=votes.encode();
  Book again;
  REQUIRE((again.decode(large.data(),large.size())));
  REQUIRE((again.blocked(pack,5,0)));
  REQUIRE((again.pairs()==kNonFitPairs));
}
