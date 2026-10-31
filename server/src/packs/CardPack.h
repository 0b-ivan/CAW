// One card pack as loaded from its source file, with the facts the table must show about it.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace caw {

struct PromptCard {
  std::string text;
  std::uint8_t pick = 1;  // answers to choose, one per blank
  std::uint8_t draw = 0;  // extra cards dealt before answering
};

struct CardPack {
  std::string id;  // stable, used in snapshots: lowercase letters, digits and '-'
  std::string title;
  std::string language;
  std::string license;
  std::string rightsHolder;
  std::string sourceUrl;
  std::vector<PromptCard> prompts;
  std::vector<std::string> answers;
};

// What a manifest says about one pack file.
struct PackEntry {
  std::string id;
  std::string file;
  std::string title;
  std::string language;
  std::string license;
  std::string rightsHolder;
  std::string sourceUrl;
};

}  // namespace caw
