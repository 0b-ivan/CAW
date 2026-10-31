// Event-loop-owned registry for M2 room identities and host credentials.
#pragma once

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace caw {

struct CreatedSession {
  std::string code;
  std::string hostToken;
};

class SessionRegistry final {
 public:
  using Clock = std::chrono::steady_clock;
  using Time = Clock::time_point;

  explicit SessionRegistry(unsigned ttlHours);

  // The host token is returned only to the creator and never by room lookup.
  CreatedSession create(std::string language, Time now = Clock::now());

  // Unknown and expired rooms are indistinguishable.
  std::optional<std::string> language(std::string_view code, Time now = Clock::now()) const;
  bool authenticatedHost(std::string_view code, std::string_view hostToken,
                         Time now = Clock::now()) const;
  std::size_t activeCount(Time now = Clock::now()) const;
  void expire(Time now = Clock::now());

 private:
  struct Room {
    std::string language;
    std::string hostToken;
    Time expiresAt;
  };

  static bool sameToken(std::string_view left, std::string_view right);

  std::chrono::hours ttl_;
  std::unordered_map<std::string, Room> rooms_;
};

}  // namespace caw
