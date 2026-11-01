#include "PackLibrary.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_set>

#include "log.h"
#include "net/Messages.h"
#include "packs/CsvPack.h"

namespace caw {
namespace {

std::optional<std::string> readFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return std::nullopt;
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

std::string joined(const std::vector<std::string>& ids) {
  std::string key;
  for (const std::string& id : ids) {
    if (!key.empty()) key += ',';
    key += id;
  }
  return key;
}

// One manifest: every entry becomes a pack, broken ones are logged and skipped.
std::size_t loadManifest(const std::filesystem::path& manifest, PackLibrary& library) {
  const auto text = readFile(manifest);
  const auto entries = text ? Messages::readPackManifest(*text) : std::nullopt;
  if (!entries) {
    log::warn("Pack manifest unreadable: " + manifest.string());
    return 0;
  }
  std::size_t loaded = 0;
  for (const PackEntry& entry : *entries) {
    CardPack pack;
    pack.id = entry.id;
    pack.title = entry.title;
    pack.language = entry.language;
    pack.license = entry.license;
    pack.rightsHolder = entry.rightsHolder;
    pack.sourceUrl = entry.sourceUrl;
    pack.defaultSelected = entry.defaultSelected;
    const auto csv = readFile(manifest.parent_path() / entry.file);
    std::string error = "file missing";
    if (!csv || !CsvPack::read(*csv, pack, error) || !library.add(std::move(pack))) {
      log::warn("Pack " + entry.id + " skipped: " + error);
      continue;
    }
    ++loaded;
  }
  return loaded;
}

}  // namespace

std::size_t PackLibrary::loadDirectory(const std::string& directory) {
  std::error_code failure;
  std::vector<std::filesystem::path> manifests;
  for (const auto& item : std::filesystem::directory_iterator(directory, failure))
    if (std::filesystem::exists(item.path() / "packs.json"))
      manifests.push_back(item.path() / "packs.json");
  // Directory order differs between file systems; sorted keeps the pack list stable.
  std::sort(manifests.begin(), manifests.end());
  std::size_t loaded = 0;
  for (const auto& manifest : manifests) loaded += loadManifest(manifest, *this);
  return loaded;
}

bool PackLibrary::validId(const std::string& id) {
  if (id.empty() || id.size() > 40) return false;
  return std::all_of(id.begin(), id.end(), [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
  });
}

bool PackLibrary::add(CardPack pack) {
  if (!validId(pack.id) || find(pack.id) != nullptr) return false;
  packs_.push_back(std::make_unique<CardPack>(std::move(pack)));
  return true;
}

const CardPack* PackLibrary::find(const std::string& id) const {
  for (const auto& pack : packs_)
    if (pack->id == id) return pack.get();
  return nullptr;
}

std::vector<std::string> PackLibrary::defaultSelection(const std::string& language) const {
  std::vector<std::string> ids;
  for (const auto& pack : packs_)
    if (pack->defaultSelected && pack->language == language) ids.push_back(pack->id);
  if (ids.empty())
    for (const auto& pack : packs_)
      if (pack->defaultSelected) ids.push_back(pack->id);
  if (ids.size() > kMaxPacks) ids.resize(kMaxPacks);
  return normalized(ids);
}

std::vector<std::string> PackLibrary::normalized(std::vector<std::string> ids) {
  std::sort(ids.begin(), ids.end());
  ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
  return ids;
}

const cardparty::Deck* PackLibrary::deck(std::vector<std::string> ids) {
  const std::size_t requested = ids.size();
  ids = normalized(std::move(ids));
  if (ids.empty() || ids.size() != requested || ids.size() > kMaxPacks) return nullptr;
  const std::string key = joined(ids);
  auto found = decks_.find(key);
  if (found == decks_.end()) {
    auto built = build(ids);
    if (!built) return nullptr;
    found = decks_.emplace(key, std::move(built)).first;
  }
  return &found->second->deck;
}

// Packs in id order; a card that is word for word in two packs is played once.
std::unique_ptr<PackLibrary::BuiltDeck> PackLibrary::build(
    const std::vector<std::string>& ids) const {
  auto built = std::make_unique<BuiltDeck>();
  std::unordered_set<std::string> seenPrompts, seenAnswers;
  for (const std::string& id : ids) {
    const CardPack* pack = find(id);
    if (pack == nullptr) return nullptr;
    for (const PromptCard& card : pack->prompts)
      if (seenPrompts.insert(card.text).second)
        built->prompts.push_back({card.text.c_str(), card.pick, card.draw});
    for (const std::string& card : pack->answers)
      if (seenAnswers.insert(card).second) built->answers.push_back({card.c_str()});
  }
  if (built->prompts.empty() || built->answers.empty() || built->prompts.size() > 4096 ||
      built->answers.size() > 4096)
    return nullptr;
  built->deck.prompts = built->prompts.data();
  built->deck.promptsCount = static_cast<std::uint16_t>(built->prompts.size());
  built->deck.answers = built->answers.data();
  built->deck.answersCount = static_cast<std::uint16_t>(built->answers.size());
  return built;
}

}  // namespace caw
