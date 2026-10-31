// Firmware regression scenarios, adapted only to the doctest harness.
#include <doctest/doctest.h>
#include <cstring>
#include "CardPartySentence.h"
using cardparty::sentence::compose;
TEST_CASE("upstream test_sentence") {
  char out[256]={};
  const char* one[]={"die Kaffeemaschine"};
  REQUIRE((compose("Heute gewinnt ____!",one,1,out,sizeof(out))>0));
  REQUIRE((std::strcmp(out,"Heute gewinnt die Kaffeemaschine!")==0));
  const char* two[]={"eine Maus","das Internet"};
  compose("____ trifft ____.",two,2,out,sizeof(out));
  REQUIRE((std::strcmp(out,"eine Maus trifft das Internet.")==0));
  const char* three[]={"A","B","C"};
  compose("[blank] und {blank} und ____.",three,3,out,sizeof(out));
  REQUIRE((std::strcmp(out,"A und B und C.")==0));
  compose("Ohne Platzhalter:",two,2,out,sizeof(out));
  REQUIRE((std::strcmp(out,"Ohne Platzhalter: eine Maus das Internet")==0));
  compose("____ + ____",one,1,out,sizeof(out));
  REQUIRE((std::strcmp(out,"die Kaffeemaschine + ____")==0));
  // Input is plain text and must remain so (never interpolated as HTML).
  const char* hostile[]={"<script>alert(1)</script>"};
  compose("____",hostile,1,out,sizeof(out));
  REQUIRE((std::strcmp(out,"<script>alert(1)</script>")==0));
  char tiny[8]={};
  compose("Hallo ____",one,1,tiny,sizeof(tiny));
  REQUIRE((tiny[sizeof(tiny)-1]==0));
  char empty[1]={};
  REQUIRE((compose("Test",one,1,empty,sizeof(empty))==0));
}
