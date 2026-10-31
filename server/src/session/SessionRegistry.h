// Event-loop-owned registry: room code → running Session, with host credentials and expiry.
#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "session/Session.h"
#include "session/SyntheticDeck.h"

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

  // The running match of a room, or nullptr. Every lookup counts as activity.
  Session* find(std::string_view code, Time now = Clock::now());
  // All running matches with their codes, for the clock tick.
  std::vector<std::pair<std::string, Session*>> all();

  // The room as Messages::saveRoom text, for Redis. Empty for an unknown code.
  std::string snapshot(const std::string& code);
  // Brings a saved room back after a restart. False for a malformed or outdated snapshot.
  bool restore(const std::string& code, std::string_view snapshot, Time now = Clock::now());

 private:
  struct Room {
    std::string language;
    std::string hostToken;
    Time expiresAt;
    std::unique_ptr<Session> session;
  };

  std::chrono::hours ttl_;
  // Placeholder cards until pack import (M5); shared read-only by every session.
  SyntheticDeck cards_;
  std::unordered_map<std::string, Room> rooms_;
};

// Milliseconds on the steady clock, the time base of every Session and Table.
std::uint32_t millisNow();

}  // namespace caw
