#include "PlayerWords.h"

#include "game/CardPartyHistory.h"

namespace caw {

const char* playerWord(std::uint8_t word) {
  static constexpr const char* kWords[kPlayerWords] = {
      "Toast",  "Keks",    "Komet", "Fuchs",      "Mond",    "Igel",   "Kompass", "Funke",
      "Birne",  "Roboter", "Kabel", "Wolke",      "Wuerfel", "Maus",   "Tasse",   "Pixel",
      "Drache", "Tinte",   "Pfeil", "Kiwi",       "Kaktus",  "Nebel",  "Wal",     "Stern",
      "Pilz",   "Dachs",   "Quarz", "Echo",       "Trommel", "Falter", "Buch",    "Knopf",
      "Feder",  "Kreisel", "Sonne", "Rabe",       "Blitz",   "Uhr",    "Kran",    "Insel",
      "Nuss",   "Luchs",   "Radio", "Bohne",      "Kiesel",  "Baum",   "Hut",     "Ninja",
      "Karte",  "Moewe",   "Fels",  "Leuchtturm", "Panda",   "Ritter", "Dingo",   "Mohn",
      "Zebra",  "Lava",    "Otter", "Segel",      "Tango",   "Tiger",  "Hase",    "Lotus"};
  return kWords[word % kPlayerWords];
}

std::uint8_t wordForPlayer(const std::string& playerId) {
  return cardparty::history::Book::wordFor(std::stoull(playerId, nullptr, 16));
}

}  // namespace caw
