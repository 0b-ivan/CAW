// Every 10th prompt is a Pick 3, every 4th a Pick 2, the rest Pick 1.
#include "SyntheticDeck.h"

namespace caw {
namespace {
constexpr int kPrompts = 60;
constexpr int kAnswers = 400;

std::string promptText(int number, int pick) {
  std::string text = "Testfrage " + std::to_string(number) + ":";
  for (int blank = 0; blank < pick; ++blank) text += " ___";
  return text + ".";
}
}  // namespace

SyntheticDeck::SyntheticDeck() {
  promptTexts_.reserve(kPrompts);
  prompts_.reserve(kPrompts);
  for (int i = 1; i <= kPrompts; ++i) {
    const int pick = i % 10 == 0 ? 3 : (i % 4 == 0 ? 2 : 1);
    const int draw = pick == 3 ? 2 : 0;
    promptTexts_.push_back(promptText(i, pick));
    prompts_.push_back({nullptr, static_cast<uint8_t>(pick), static_cast<uint8_t>(draw)});
  }
  answerTexts_.reserve(kAnswers);
  answers_.reserve(kAnswers);
  for (int i = 1; i <= kAnswers; ++i) {
    answerTexts_.push_back("Testantwort " + std::to_string(i));
    answers_.push_back({nullptr});
  }
  // Pointers are taken only after the vectors stop growing.
  for (int i = 0; i < kPrompts; ++i) prompts_[i].text = promptTexts_[i].c_str();
  for (int i = 0; i < kAnswers; ++i) answers_[i].text = answerTexts_[i].c_str();
  deck_.prompts = prompts_.data();
  deck_.promptsCount = static_cast<uint16_t>(prompts_.size());
  deck_.answers = answers_.data();
  deck_.answersCount = static_cast<uint16_t>(answers_.size());
}

}  // namespace caw
