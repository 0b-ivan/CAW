// Keep room state on the single uWebSockets event-loop thread.
#include "SessionRegistry.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

#include "Codes.h"
#include "net/Messages.h"

namespace caw {
namespace {
std::string customPrefix(std::string_view code) {
  std::string prefix = "custom-";
  for (unsigned char letter : code) prefix += static_cast<char>(std::tolower(letter));
  return prefix + "-";
}
}  // namespace

std::uint32_t millisNow() {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  static const auto start = std::chrono::steady_clock::now();
  // Wraps after 49 days; the Table compares times wrap-safely.
  return static_cast<std::uint32_t>(
      duration_cast<milliseconds>(std::chrono::steady_clock::now() - start).count());
}

SessionRegistry::SessionRegistry(unsigned ttlHours, PackLibrary& packs)
    : ttl_(ttlHours), packs_(packs) {
  if (ttlHours == 0) throw std::invalid_argument("Session TTL must be positive");
}

CreatedSession SessionRegistry::create(std::string language, Time now) {
  if (language != "de" && language != "en")
    throw std::invalid_argument("Unsupported session language");
  expire(now);
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    CreatedSession result{Codes::sessionCode(), Codes::hostToken()};
    if (rooms_.count(result.code) != 0) continue;
    // A new room starts with every pack in its language; the host can change that in the lobby.
    auto session = std::make_unique<Session>(
        result.hostToken, packs_, packs_.defaultSelection(language), Codes::seed(), millisNow(),
        customPrefix(result.code));
    rooms_.emplace(result.code, Room{language, result.hostToken, now + ttl_, std::move(session),
                                     std::nullopt});
    return result;
  }
  throw std::runtime_error("Unable to allocate unique session code");
}

std::optional<std::string> SessionRegistry::language(std::string_view code, Time now) const {
  if (!Codes::validSessionCode(code)) return std::nullopt;
  const auto found = rooms_.find(std::string(code));
  if (found == rooms_.end() || now >= found->second.expiresAt) return std::nullopt;
  return found->second.language;
}

bool SessionRegistry::authenticatedHost(std::string_view code, std::string_view hostToken,
                                        Time now) const {
  if (!Codes::validSessionCode(code)) return false;
  const auto found = rooms_.find(std::string(code));
  if (found == rooms_.end() || now >= found->second.expiresAt) return false;
  return Codes::sameToken(found->second.hostToken, hostToken);
}

const CardPack* SessionRegistry::customCards(std::string_view code) const {
  const auto found = rooms_.find(std::string(code));
  if (found == rooms_.end() || !found->second.customPack) return nullptr;
  return &*found->second.customPack;
}

bool SessionRegistry::updateCustomCards(std::string_view code, CardPack pack) {
  Session* session = find(code);
  if (session == nullptr || !session->inLobby()) return false;
  Room& room = rooms_.at(std::string(code));
  std::vector<std::string> selected = session->packs();
  if (room.customPack) {
    const std::string& oldId = room.customPack->id;
    selected.erase(std::remove(selected.begin(), selected.end(), oldId), selected.end());
  }
  if (selected.size() >= PackLibrary::kMaxPacks) return false;
  // Immutable version per edit: no live deck ever points to replaced card text.
  std::string suffix = Codes::sessionCode();
  for (char& letter : suffix)
    letter = static_cast<char>(std::tolower(static_cast<unsigned char>(letter)));
  const std::string id = customPrefix(code) + suffix;
  pack.id = id;
  pack.language = room.language;
  pack.title = room.language == "en" ? "Custom cards" : "Eigene Karten";
  if (!packs_.add(std::move(pack))) return false;
  selected.push_back(id);
  if (!session->activatePacks(selected, millisNow())) {
    packs_.remove(id);
    return false;
  }
  if (room.customPack) packs_.remove(room.customPack->id);
  room.customPack = *packs_.find(id);
  return true;
}

std::size_t SessionRegistry::activeCount(Time now) const {
  std::size_t count = 0;
  for (const auto& [code, room] : rooms_) {
    (void)code;
    if (now < room.expiresAt) ++count;
  }
  return count;
}

void SessionRegistry::expire(Time now) {
  for (auto room = rooms_.begin(); room != rooms_.end();) {
    if (now >= room->second.expiresAt) {
      const std::string oldId = room->second.customPack ? room->second.customPack->id : "";
      room = rooms_.erase(room);
      if (!oldId.empty()) packs_.remove(oldId);
    }
    else
      ++room;
  }
}

Session* SessionRegistry::find(std::string_view code, Time now) {
  if (!Codes::validSessionCode(code)) return nullptr;
  const auto found = rooms_.find(std::string(code));
  if (found == rooms_.end() || now >= found->second.expiresAt) return nullptr;
  found->second.expiresAt = now + ttl_;
  return found->second.session.get();
}

std::vector<std::pair<std::string, Session*>> SessionRegistry::all() {
  std::vector<std::pair<std::string, Session*>> sessions;
  sessions.reserve(rooms_.size());
  for (auto& [code, room] : rooms_) sessions.emplace_back(code, room.session.get());
  return sessions;
}

std::string SessionRegistry::snapshot(const std::string& code) {
  const auto found = rooms_.find(code);
  if (found == rooms_.end()) return {};
  const Room& room = found->second;
  const Session& session = *room.session;
  return Messages::saveRoom({room.language, room.hostToken, session.packs(),
                             session.saveState(millisNow()), session.identities(),
                             session.matchLines(), session.locked(), room.customPack});
}

bool SessionRegistry::restore(const std::string& code, std::string_view snapshot, Time now) {
  const auto record = Messages::loadRoom(snapshot);
  if (!Codes::validSessionCode(code) || !record || !Codes::validHostToken(record->hostToken) ||
      (record->language != "de" && record->language != "en") || rooms_.count(code) != 0)
    return false;
  // Restore private cards before rebuilding the deck fingerprint and card indices.
  std::string customId;
  if (record->customPack) {
    customId = record->customPack->id;
    if (!PackLibrary::validId(customId) || !customId.starts_with(customPrefix(code)))
      return false;
    record->customPack->language = record->language;
    record->customPack->title = record->language == "en" ? "Custom cards" : "Eigene Karten";
    if (!packs_.add(*record->customPack)) return false;
  }
  if (packs_.deck(record->packs) == nullptr) {
    if (!customId.empty()) packs_.remove(customId);
    return false;
  }
  auto session = std::make_unique<Session>(record->hostToken, packs_, record->packs, Codes::seed(),
                                           millisNow(), customPrefix(code));
  if (!session->loadState(record->state, millisNow())) {
    if (!customId.empty()) packs_.remove(customId);
    return false;
  }
  session->restoreExtras(record->identities, record->lines, record->locked);
  // Restored is not a change: it is already saved exactly like this.
  session->takeChanged();
  rooms_.emplace(code, Room{record->language, record->hostToken, now + ttl_,
                            std::move(session), record->customPack});
  return true;
}

}  // namespace caw
