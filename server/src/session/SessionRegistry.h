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

#include "packs/PackLibrary.h"
#include "session/Session.h"
#include "session/RoomExtras.h"

namespace caw {

struct CreatedSession {
  std::string code;
  std::string hostToken;
};

class SessionRegistry final {
 public:
  using Clock = std::chrono::steady_clock;
  using Time = Clock::time_point;

  // The pack library must outlive the registry.
  SessionRegistry(unsigned ttlHours, PackLibrary& packs);

  // The host token is returned only to the creator and never by room lookup.
  CreatedSession create(std::string language, Time now = Clock::now());

  // Unknown and expired rooms are indistinguishable.
  std::optional<std::string> language(std::string_view code, Time now = Clock::now()) const;
  bool authenticatedHost(std::string_view code, std::string_view hostToken,
                         Time now = Clock::now()) const;
  std::size_t activeCount(Time now = Clock::now()) const;
  void expire(Time now = Clock::now());
  // The room-only pack is private and may be replaced only in the lobby.
  const CardPack* customCards(std::string_view code) const;
  bool updateCustomCards(std::string_view code, CardPack pack);
  const std::vector<CardProposal>& proposals(std::string_view code) const;
  bool submitProposal(std::string_view code, std::string_view ticket, CardProposal proposal);
  bool reviewProposal(std::string_view code, std::string_view id, bool approve);

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
    std::optional<CardPack> customPack = std::nullopt;
    std::vector<CardProposal> proposals;
  };

  std::chrono::hours ttl_;
  PackLibrary& packs_;
  std::unordered_map<std::string, Room> rooms_;
};

// Milliseconds on the steady clock, the time base of every Session and Table.
std::uint32_t millisNow();

}  // namespace caw
