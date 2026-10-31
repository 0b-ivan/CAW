// Numbered placeholder cards so the server can run full matches before pack import (M5).
#pragma once

#include <string>
#include <vector>

#include "game/CardPartyCore.h"

namespace caw {

class SyntheticDeck final {
 public:
  SyntheticDeck();
  SyntheticDeck(const SyntheticDeck&) = delete;
  SyntheticDeck& operator=(const SyntheticDeck&) = delete;

  // Points into this object's strings: keep the SyntheticDeck alive while a game uses it.
  const cardparty::Deck& deck() const { return deck_; }

 private:
  std::vector<std::string> promptTexts_;
  std::vector<std::string> answerTexts_;
  std::vector<cardparty::Prompt> prompts_;
  std::vector<cardparty::Answer> answers_;
  cardparty::Deck deck_{};
};

}  // namespace caw
