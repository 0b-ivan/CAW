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
