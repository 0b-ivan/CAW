// Keep room state on the single uWebSockets event-loop thread.
#include "SessionRegistry.h"

#include <stdexcept>
#include <utility>

#include "Codes.h"

namespace caw {

std::uint32_t millisNow() {
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  static const auto start = std::chrono::steady_clock::now();
  // Wraps after 49 days; the Table compares times wrap-safely.
  return static_cast<std::uint32_t>(
      duration_cast<milliseconds>(std::chrono::steady_clock::now() - start).count());
}

SessionRegistry::SessionRegistry(unsigned ttlHours) : ttl_(ttlHours) {
  if (ttlHours == 0) throw std::invalid_argument("Session TTL must be positive");
}

CreatedSession SessionRegistry::create(std::string language, Time now) {
  if (language != "de" && language != "en")
    throw std::invalid_argument("Unsupported session language");
  expire(now);
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    CreatedSession result{Codes::sessionCode(), Codes::hostToken()};
    if (rooms_.count(result.code) != 0) continue;
    auto session =
        std::make_unique<Session>(result.hostToken, cards_.deck(), Codes::seed(), millisNow());
    rooms_.emplace(result.code, Room{language, result.hostToken, now + ttl_, std::move(session)});
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
    if (now >= room->second.expiresAt)
      room = rooms_.erase(room);
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

std::vector<Session*> SessionRegistry::all() {
  std::vector<Session*> sessions;
  sessions.reserve(rooms_.size());
  for (auto& [code, room] : rooms_) {
    (void)code;
    sessions.push_back(room.session.get());
  }
  return sessions;
}

}  // namespace caw
