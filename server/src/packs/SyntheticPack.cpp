#include "SyntheticPack.h"

namespace caw {

CardPack syntheticPack() {
  CardPack pack;
  pack.id = "test";
  pack.title = "Testkarten";
  pack.language = "de";
  pack.license = "CC0";
  pack.rightsHolder = "CAW";
  for (int i = 1; i <= 60; ++i) {
    const int pick = i % 10 == 0 ? 3 : (i % 4 == 0 ? 2 : 1);
    std::string text = "Testfrage " + std::to_string(i) + ":";
    for (int blank = 0; blank < pick; ++blank) text += " ___";
    pack.prompts.push_back({text + ".", static_cast<std::uint8_t>(pick),
                            static_cast<std::uint8_t>(pick == 3 ? 2 : 0)});
  }
  for (int i = 1; i <= 400; ++i) pack.answers.push_back("Testantwort " + std::to_string(i));
  return pack;
}

}  // namespace caw
