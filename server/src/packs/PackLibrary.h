// All card packs the server knows, and the decks built from chosen packs.
// Lives as long as the server; sessions point into its decks.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "game/CardPartyCore.h"
#include "packs/CardPack.h"

namespace caw {

class PackLibrary final {
 public:
  // Like the X4's deck set.
  static constexpr std::size_t kMaxPacks = 8;

  PackLibrary() = default;
  PackLibrary(const PackLibrary&) = delete;
  PackLibrary& operator=(const PackLibrary&) = delete;

  // Loads every packs.json one level below `directory` and the CSV files it names.
  // A broken pack is logged and skipped; returns how many packs were loaded.
  std::size_t loadDirectory(const std::string& directory);
  // False for an invalid or duplicate id.
  bool add(CardPack pack);

  const std::vector<std::unique_ptr<CardPack>>& packs() const { return packs_; }
  const CardPack* find(const std::string& id) const;

  // Eligible packs in this language, or other eligible packs if none match.
  // Adult/optional packs are visible but are never selected by default.
  std::vector<std::string> defaultSelection(const std::string& language) const;

  // The deck for these packs, built once and then reused. Ids are taken in sorted
  // order, so the same choice always gives the same card numbers (snapshots rely on
  // that). Nullptr for an unknown id, a duplicate, more than kMaxPacks, or a choice
  // without prompts or answers.
  const cardparty::Deck* deck(std::vector<std::string> ids);

  // Sorted and without duplicates, as deck() and snapshots use them.
  static std::vector<std::string> normalized(std::vector<std::string> ids);
  static bool validId(const std::string& id);

 private:
  // One built combination: the game's card arrays point into the packs' strings.
  struct BuiltDeck {
    std::vector<cardparty::Prompt> prompts;
    std::vector<cardparty::Answer> answers;
    cardparty::Deck deck;
  };

  std::unique_ptr<BuiltDeck> build(const std::vector<std::string>& ids) const;

  // unique_ptr keeps the strings in place when more packs are added.
  std::vector<std::unique_ptr<CardPack>> packs_;
  std::map<std::string, std::unique_ptr<BuiltDeck>> decks_;
};

}  // namespace caw
