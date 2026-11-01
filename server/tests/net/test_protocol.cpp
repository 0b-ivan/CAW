// The live protocol accepts exactly the documented commands and nothing else.
#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include "net/Protocol.h"

using caw::Command;
using caw::Protocol;

namespace {
const std::string kToken = "\"0123456789abcdef0123456789abcdef\"";
}

TEST_CASE("valid commands are read with their fields") {
  auto join = Protocol::parse(R"({"type":"join","name":"Ada"})");
  REQUIRE(join);
  CHECK(join->type == Command::Type::Join);
  CHECK(join->name == "Ada");

  auto submit =
      Protocol::parse(R"({"type":"submit","token":)" + kToken + R"(,"revision":3,"slots":[2,0]})");
  REQUIRE(submit);
  CHECK(submit->revision == 3);
  CHECK(submit->slots == std::vector<std::uint8_t>{2, 0});

  auto kick = Protocol::parse(R"({"type":"kick","index":4})");
  REQUIRE(kick);
  CHECK(kick->type == Command::Type::Kick);
  CHECK(kick->index == 4);

  CHECK(Protocol::parse(R"({"type":"start"})"));

  // The device id is optional on join and must be 16 hex characters.
  auto withId = Protocol::parse(R"({"type":"join","name":"Ada","player":"0123456789abcdef"})");
  REQUIRE(withId);
  CHECK(withId->player == "0123456789abcdef");
  CHECK_FALSE(Protocol::parse(R"({"type":"join","name":"Ada","player":"xyz"})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"join","name":"Ada","player":"0000000000000000"})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"start","player":"0123456789abcdef"})"));
}

TEST_CASE("anything else is refused") {
  CHECK_FALSE(Protocol::parse(""));
  CHECK_FALSE(Protocol::parse("not json"));
  CHECK_FALSE(Protocol::parse(R"([])"));
  CHECK_FALSE(Protocol::parse(R"({"type":"dance"})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"start","extra":1})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"join"})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"join","name":"123456789012345678901234"})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"resume","token":"short"})"));
  CHECK_FALSE(
      Protocol::parse(R"({"type":"submit","token":)" + kToken + R"(,"revision":-1,"slots":[0]})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"submit","token":)" + kToken +
                              R"(,"revision":1,"slots":[0,1,2,3]})"));
  CHECK_FALSE(
      Protocol::parse(R"({"type":"submit","token":)" + kToken + R"(,"revision":1,"slots":[]})"));
  CHECK_FALSE(Protocol::parse(R"({"type":"kick","index":"4"})"));
  CHECK_FALSE(Protocol::parse(std::string(Protocol::kMaxMessageBytes + 1, ' ')));
}

TEST_CASE("notices and errors are small typed frames") {
  CHECK(nlohmann::json::parse(Protocol::error("rejected")) ==
        nlohmann::json({{"type", "error"}, {"code", "rejected"}}));
  CHECK(nlohmann::json::parse(Protocol::notice("expired")) ==
        nlohmann::json({{"type", "expired"}}));
}

TEST_CASE("settings accept hand size 3 through 10 and the existing timer choices") {
  const std::string good =
      R"({"type":"settings","settings":{"hand":7,"rounds":5,"answerSecs":60,"extensionSecs":15,)"
      R"("jurorSecs":30,"autoSecs":8,"rando":false}})";
  auto settings = Protocol::parse(good);
  REQUIRE(settings);
  CHECK(settings->type == Command::Type::Settings);
  CHECK(settings->settings.hand == 7);
  CHECK(settings->settings.rounds == 5);
  CHECK(settings->settings.answerSecs == 60);
  CHECK(settings->settings.jurorSecs == 30);
  CHECK(settings->settings.autoSecs == 8);
  CHECK_FALSE(settings->settings.rando);

  auto changed = [&](const std::string& from, const std::string& to) {
    std::string text = good;
    text.replace(text.find(from), from.size(), to);
    return text;
  };
  CHECK_FALSE(Protocol::parse(changed("\"rounds\":5", "\"rounds\":7")));
  CHECK_FALSE(Protocol::parse(changed("\"hand\":7", "\"hand\":2")));
  CHECK_FALSE(Protocol::parse(changed("\"hand\":7", "\"hand\":11")));
  CHECK_FALSE(Protocol::parse(changed("\"hand\":7", "\"hand\":\"7\"")));
  CHECK_FALSE(Protocol::parse(changed("\"hand\":7,", "")));
  for (unsigned hand = 3; hand <= 10; ++hand) {
    auto value = Protocol::parse(changed("\"hand\":7", "\"hand\":" + std::to_string(hand)));
    REQUIRE(value);
    CHECK(value->settings.hand == hand);
  }
  CHECK_FALSE(Protocol::parse(changed("\"answerSecs\":60", "\"answerSecs\":61")));
  CHECK_FALSE(Protocol::parse(changed("\"rando\":false", "\"rando\":0")));
  CHECK_FALSE(Protocol::parse(changed(",\"rando\":false", "")));
  CHECK_FALSE(Protocol::parse(changed("\"rando\":false", "\"rando\":false,\"x\":1")));
}
