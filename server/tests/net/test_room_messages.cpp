// Test the strict public M2 session REST protocol and reject malformed bodies.
#include <doctest/doctest.h>

#include <string>

#include "net/Messages.h"

TEST_CASE("Session creation accepts only a small JSON object") {
  CHECK(caw::Messages::createLanguage("{}") == "de");
  CHECK(caw::Messages::createLanguage(R"({"language":"de"})") == "de");
  CHECK(caw::Messages::createLanguage(R"({"language":"en"})") == "en");
  CHECK_FALSE(caw::Messages::createLanguage("").has_value());
  CHECK_FALSE(caw::Messages::createLanguage("[]").has_value());
  CHECK_FALSE(caw::Messages::createLanguage(R"({"language":"fr"})").has_value());
  CHECK_FALSE(caw::Messages::createLanguage(R"({"language":null})").has_value());
  CHECK_FALSE(caw::Messages::createLanguage(R"({"language":"de","host":true})").has_value());
  CHECK_FALSE(caw::Messages::createLanguage("{malformed").has_value());
  CHECK_FALSE(caw::Messages::createLanguage(std::string(1025, ' ')).has_value());
}

TEST_CASE("Public room metadata never carries host tokens") {
  const std::string secret = "0123456789abcdef0123456789abcdef";
  const std::string created = caw::Messages::sessionCreated("ABC234", secret);
  const std::string publicRoom = caw::Messages::publicRoom("ABC234", "en");
  CHECK(created.find(secret) != std::string::npos);
  CHECK(publicRoom.find(secret) == std::string::npos);
  CHECK(publicRoom.find(R"("phase":"lobby")") != std::string::npos);
  CHECK(publicRoom.find(R"("language":"en")") != std::string::npos);
  CHECK(caw::Messages::errorResponse("invalid_request") == R"({"error":"invalid_request"})");
}

TEST_CASE("custom cards use the same syntax limits as CSV packs") {
  const auto cards = caw::Messages::readCustomCards(
      R"({"prompts":["Test ___ und ___ und ___."],"answers":["Antwort, \"Zitat\""]})");
  REQUIRE(cards);
  REQUIRE(cards->prompts.size() == 1);
  CHECK(cards->prompts[0].pick == 3);
  CHECK(cards->prompts[0].draw == 2);
  REQUIRE(cards->answers.size() == 1);
  CHECK(cards->answers[0] == "Antwort, \"Zitat\"");
  CHECK_FALSE(caw::Messages::readCustomCards(R"({"prompts":[],"answers":[]})"));
  CHECK_FALSE(caw::Messages::readCustomCards(R"({"prompts":["___ ___ ___ ___"],"answers":[]})"));
  CHECK_FALSE(caw::Messages::readCustomCards(R"({"prompts":["A"],"answers":[],"host":"secret"})"));
  CHECK_FALSE(caw::Messages::readCustomCards(R"({"prompts":[3],"answers":[]})"));
  CHECK_FALSE(caw::Messages::readCustomCards(R"({"prompts":["A"],"answers":[""]})"));
  CHECK_FALSE(caw::Messages::readCustomCards(R"({"prompts":["A"],"answers":["B\tC"]})"));
  const std::string tooLong = R"({"prompts":["A"],"answers":[")" + std::string(301, 'x') + R"("]})";
  CHECK_FALSE(caw::Messages::readCustomCards(tooLong));
  CHECK_FALSE(caw::Messages::readCustomCards("not json"));
  CHECK(caw::Messages::customCards(nullptr) == R"({"answers":[],"prompts":[]})");
}
