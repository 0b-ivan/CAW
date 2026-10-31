#pragma once

// Synthetic test fixture with upstream card counts, pick and draw values.
#include "CardPartyCore.h"

namespace cardparty {

inline const Deck& demoDeck() {
  static const Prompt black[] = {
      {"Testfrage 1 ____", 1, 0},         {"Testfrage 2 ____", 1, 0},
      {"Testfrage 3 ____", 1, 0},         {"Testfrage 4 ____ / ____", 2, 0},
      {"Testfrage 5 ____ / ____", 2, 0},  {"Testfrage 6 ____", 1, 0},
      {"Testfrage 7 ____", 1, 0},         {"Testfrage 8 ____ / ____ / ____", 3, 2},
      {"Testfrage 9 ____", 1, 0},         {"Testfrage 10 ____", 1, 0},
      {"Testfrage 11 ____ / ____", 2, 0}, {"Testfrage 12 ____", 1, 0}};
  static const Answer white[] = {
      {"Testantwort 1."},  {"Testantwort 2."},  {"Testantwort 3."},  {"Testantwort 4."},
      {"Testantwort 5."},  {"Testantwort 6."},  {"Testantwort 7."},  {"Testantwort 8."},
      {"Testantwort 9."},  {"Testantwort 10."}, {"Testantwort 11."}, {"Testantwort 12."},
      {"Testantwort 13."}, {"Testantwort 14."}, {"Testantwort 15."}, {"Testantwort 16."},
      {"Testantwort 17."}, {"Testantwort 18."}, {"Testantwort 19."}, {"Testantwort 20."},
      {"Testantwort 21."}, {"Testantwort 22."}, {"Testantwort 23."}, {"Testantwort 24."},
      {"Testantwort 25."}, {"Testantwort 26."}, {"Testantwort 27."}, {"Testantwort 28."},
      {"Testantwort 29."}, {"Testantwort 30."}, {"Testantwort 31."}, {"Testantwort 32."},
      {"Testantwort 33."}, {"Testantwort 34."}, {"Testantwort 35."}, {"Testantwort 36."},
      {"Testantwort 37."}, {"Testantwort 38."}, {"Testantwort 39."}, {"Testantwort 40."},
      {"Testantwort 41."}, {"Testantwort 42."}, {"Testantwort 43."}, {"Testantwort 44."},
      {"Testantwort 45."}, {"Testantwort 46."}, {"Testantwort 47."}, {"Testantwort 48."},
      {"Testantwort 49."}, {"Testantwort 50."}, {"Testantwort 51."}, {"Testantwort 52."},
      {"Testantwort 53."}, {"Testantwort 54."}, {"Testantwort 55."}, {"Testantwort 56."},
      {"Testantwort 57."}, {"Testantwort 58."}, {"Testantwort 59."}, {"Testantwort 60."},
      {"Testantwort 61."}, {"Testantwort 62."}, {"Testantwort 63."}, {"Testantwort 64."},
      {"Testantwort 65."}, {"Testantwort 66."}, {"Testantwort 67."}, {"Testantwort 68."},
      {"Testantwort 69."}, {"Testantwort 70."}, {"Testantwort 71."}, {"Testantwort 72."},
      {"Testantwort 73."}, {"Testantwort 74."}, {"Testantwort 75."}, {"Testantwort 76."},
      {"Testantwort 77."}, {"Testantwort 78."}, {"Testantwort 79."}, {"Testantwort 80."},
      {"Testantwort 81."}, {"Testantwort 82."}, {"Testantwort 83."}, {"Testantwort 84."},
      {"Testantwort 85."}, {"Testantwort 86."}, {"Testantwort 87."}, {"Testantwort 88."},
      {"Testantwort 89."}, {"Testantwort 90."}, {"Testantwort 91."}, {"Testantwort 92."},
      {"Testantwort 93."}, {"Testantwort 94."}, {"Testantwort 95."}, {"Testantwort 96."},
      {"Testantwort 97."}, {"Testantwort 98."}, {"Testantwort 99."}, {"Testantwort 100."}};
  static const Deck deck{black, static_cast<uint16_t>(sizeof black / sizeof black[0]), white,
                         static_cast<uint16_t>(sizeof white / sizeof white[0])};
  return deck;
}
}  // namespace cardparty
