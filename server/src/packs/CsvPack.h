// Reads the Cards vs Humanity CSV format: header "typ,text", "b" for prompts, "w" for answers.
#pragma once

#include <string>
#include <string_view>

#include "packs/CardPack.h"

namespace caw {

class CsvPack final {
 public:
  static constexpr std::size_t kMaxCardBytes = 300;

  // Adds the cards of `text` to `pack`. On any bad line nothing is added and
  // `error` names the line, so a broken file never half-loads.
  static bool read(std::string_view text, CardPack& pack, std::string& error);

  // Pick = blanks ("___" or longer); a prompt without a blank takes one answer at the end.
  static PromptCard prompt(std::string text);
};

}  // namespace caw
