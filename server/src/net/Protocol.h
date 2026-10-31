// Live WebSocket protocol: read player and host commands, write state frames.
#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "game/CardPartyCore.h"
#include "game/CardPartyTable.h"
#include "packs/CardPack.h"

namespace caw {

struct Command {
  enum class Type {
    Join,     // name
    Resume,   // token = seat ticket
    Host,     // token = host token; marks the connection as host
    Submit,   // token, revision, slots
    Judge,    // token, revision, index = chosen play
    Swap,     // token, revision, index = hand slot
    Pause,    // token
    Unpause,  // token
    Again,    // token
    Start,    // host only, as all below
    Next,
    Close,
    Abort,
    Kick,  // index = seat
    End,
    Rematch,
    HostPause,
    HostUnpause,
    AddTime,
    ContinueWithout,
    Settings,  // host only: settings
    Packs,     // host only, lobby only: packs
  };
  Type type = Type::Join;
  std::string name;
  std::string token;
  std::uint32_t revision = 0;
  std::vector<std::uint8_t> slots;
  std::uint8_t index = 0;
  cardparty::Settings settings;
  std::vector<std::string> packs;
};

// Everything one connection may see. The hand is filled only for that connection's own seat.
struct StateFrame {
  const cardparty::Deck* deck = nullptr;
  const cardparty::Table* table = nullptr;
  cardparty::PublicView view;
  std::uint8_t rounds = 0;
  std::uint32_t now = 0;
  std::uint8_t seat = cardparty::kNone;  // kNone: a host connection without a seat
  bool isHost = false;
  cardparty::HandView hand;
  std::uint32_t revision = 0;
  cardparty::Settings settings;
  std::uint8_t requestedRounds = 0;    // the host's choice; `rounds` is the fair count once started
  std::vector<const CardPack*> packs;  // the packs in play, for credits on every screen
};

class Protocol final {
 public:
  static constexpr std::size_t kMaxMessageBytes = 1024;

  // Nothing for unknown types, unknown fields, wrong types or oversized values.
  static std::optional<Command> parse(std::string_view text);

  static std::string state(const StateFrame& frame);
  // The same table for a big screen: type "screen", no "me", never a hand.
  static std::string screen(const StateFrame& frame);
  static std::string welcome(std::string_view ticket, std::uint8_t seat);
  static std::string error(std::string_view code);
  // A frame that only carries its type: "expired", "kicked", "host".
  static std::string notice(std::string_view type);
};

}  // namespace caw
