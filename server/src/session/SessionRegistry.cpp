// Keep room state on the single uWebSockets event-loop thread.
#include "SessionRegistry.h"

#include <stdexcept>
#include <utility>

#include "Codes.h"

namespace caw {

SessionRegistry::SessionRegistry(unsigned ttlHours) : ttl_(ttlHours) {
  if (ttlHours == 0) throw std::invalid_argument("Session TTL must be positive");
}

CreatedSession SessionRegistry::create(std::string language, Time now) {
  if (language != "de" && language != "en")
    throw std::invalid_argument("Unsupported session language");
  expire(now);
  for (unsigned attempt = 0; attempt < 32; ++attempt) {
    CreatedSession result{Codes::sessionCode(), Codes::hostToken()};
    Room room{language, result.hostToken, now + ttl_};
    const bool added = rooms_.emplace(result.code, std::move(room)).second;
    if (added) return result;
  }
  throw std::runtime_error("Unable to allocate unique session code");
}

std::optional<std::string> SessionRegistry::language(std::string_view code, Time now) const {
  if (!Codes::validSessionCode(code)) return std::nullopt;
  const auto found = rooms_.find(std::string(code));
  if (found == rooms_.end() || now >= found->second.expiresAt) return std::nullopt;
  return found->second.language;
}

bool SessionRegistry::sameToken(std::string_view left, std::string_view right) {
  if (!Codes::validHostToken(left) || !Codes::validHostToken(right)) return false;
  unsigned char difference = 0;
  for (std::size_t i = 0; i < left.size(); ++i)
    difference |= static_cast<unsigned char>(left[i] ^ right[i]);
  return difference == 0;
}

bool SessionRegistry::authenticatedHost(std::string_view code, std::string_view hostToken,
                                        Time now) const {
  if (!Codes::validSessionCode(code)) return false;
  const auto found = rooms_.find(std::string(code));
  if (found == rooms_.end() || now >= found->second.expiresAt) return false;
  return sameToken(found->second.hostToken, hostToken);
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

}  // namespace caw
