// Test the M2 room-code generator and event-loop-owned registry.
#include <doctest/doctest.h>

#include <chrono>
#include <set>
#include <stdexcept>
#include <string>

#include "session/Codes.h"
#include "session/SessionRegistry.h"

TEST_CASE("Room codes use the unambiguous 31-symbol alphabet") {
  std::set<std::string> seen;
  for (unsigned i = 0; i < 256; ++i) {
    const std::string code = caw::Codes::sessionCode();
    CHECK(code.size() == 6);
    CHECK(caw::Codes::validSessionCode(code));
    CHECK(seen.insert(code).second);
  }
  CHECK_FALSE(caw::Codes::validSessionCode("ABC0EF"));
  CHECK_FALSE(caw::Codes::validSessionCode("ABC1EF"));
  CHECK_FALSE(caw::Codes::validSessionCode("ABCOEF"));
  CHECK_FALSE(caw::Codes::validSessionCode("ABIIEF"));
  CHECK_FALSE(caw::Codes::validSessionCode("ABCDEF7"));
  CHECK_FALSE(caw::Codes::validSessionCode("abcdef"));
}

TEST_CASE("Host tokens are random 128-bit lowercase hex credentials") {
  const std::string first = caw::Codes::hostToken();
  const std::string second = caw::Codes::hostToken();
  CHECK(first.size() == 32);
  CHECK(caw::Codes::validHostToken(first));
  CHECK(caw::Codes::validHostToken(second));
  CHECK(first != second);
  CHECK_FALSE(caw::Codes::validHostToken("123456789"));
  CHECK_FALSE(caw::Codes::validHostToken("GGGGGGGGGGGGGGGGGGGGGGGGGGGGGGGG"));
}

TEST_CASE("Only the creator receives the host credential") {
  caw::SessionRegistry registry(24);
  const auto now = caw::SessionRegistry::Clock::now();
  const auto first = registry.create("de", now);
  const auto second = registry.create("en", now);
  REQUIRE(first.code != second.code);
  REQUIRE(first.hostToken != second.hostToken);
  CHECK(registry.activeCount(now) == 2);
  CHECK(registry.language(first.code, now) == "de");
  CHECK(registry.language(second.code, now) == "en");
  CHECK_FALSE(registry.language("ABC0EF", now).has_value());
  CHECK_FALSE(registry.language("ZZZZZZ", now).has_value());
  CHECK(registry.authenticatedHost(first.code, first.hostToken, now));
  CHECK_FALSE(registry.authenticatedHost(second.code, first.hostToken, now));
  CHECK_FALSE(registry.authenticatedHost(first.code, second.hostToken, now));
  CHECK_FALSE(registry.authenticatedHost(first.code, "invalid", now));
}

TEST_CASE("Expiry revokes both lookup and host privileges") {
  caw::SessionRegistry registry(24);
  const auto start = caw::SessionRegistry::Clock::now();
  const auto room = registry.create("de", start);
  const auto before = start + std::chrono::hours(24) - std::chrono::milliseconds(1);
  const auto after = start + std::chrono::hours(24);
  CHECK(registry.language(room.code, before).has_value());
  CHECK(registry.authenticatedHost(room.code, room.hostToken, before));
  CHECK_FALSE(registry.language(room.code, after).has_value());
  CHECK_FALSE(registry.authenticatedHost(room.code, room.hostToken, after));
  CHECK(registry.activeCount(after) == 0);
  registry.expire(after);
  CHECK(registry.activeCount(after) == 0);
}

TEST_CASE("Unsupported languages and invalid TTL are rejected") {
  CHECK_THROWS_AS(caw::SessionRegistry(0), std::invalid_argument);
  caw::SessionRegistry registry(24);
  CHECK_THROWS_AS(registry.create("fr"), std::invalid_argument);
  CHECK(registry.activeCount() == 0);
}
