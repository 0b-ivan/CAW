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
  const auto cleared = caw::Messages::readCustomCards(R"({"prompts":[],"answers":[]})");
  REQUIRE(cleared);
  CHECK(cleared->prompts.empty());
  CHECK(cleared->answers.empty());
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

TEST_CASE("player proposals reuse card validation and strict review decisions") {
  const auto prompt = caw::Messages::readProposal(
      R"({"type":"prompt","text":"Synthese ___ und ___ und ___."})");
  REQUIRE(prompt);
  CHECK(prompt->type == "prompt");
  CHECK(prompt->text == "Synthese ___ und ___ und ___.");
  const auto answer = caw::Messages::readProposal(
      R"({"type":"answer","text":"Ein synthetischer Vorschlag."})");
  REQUIRE(answer);
  CHECK(answer->type == "answer");
  CHECK_FALSE(caw::Messages::readProposal(R"({"type":"prompt","text":"___ ___ ___ ___"})"));
  CHECK_FALSE(caw::Messages::readProposal(R"({"type":"answer","text":""})"));
  CHECK_FALSE(caw::Messages::readProposal(R"({"type":"other","text":"Test"})"));
  CHECK_FALSE(caw::Messages::readProposal(R"({"type":"answer","text":"Test","token":"x"})"));
  CHECK_FALSE(caw::Messages::readProposal(R"({"type":"prompt","text":42})"));
  CHECK_FALSE(caw::Messages::readProposal("[]"));
  const std::string oversized =
      R"({"type":"answer","text":")" + std::string(301, 'a') + R"("})";
  CHECK_FALSE(caw::Messages::readProposal(oversized));

  auto queueItem = *prompt;
  queueItem.id = "ABC234";
  queueItem.author = "Ada";
  queueItem.seat = 1;
  const std::string queue = caw::Messages::proposalQueue({queueItem});
  CHECK(queue.find("Synthese") != std::string::npos);
  CHECK(queue.find(R"("author":"Ada")") != std::string::npos);
  CHECK(queue.find("seat") == std::string::npos);
  CHECK(queue.find("hostToken") == std::string::npos);

  const auto decision = caw::Messages::readProposalDecision(R"({"id":"ABC234","approve":true})");
  REQUIRE(decision);
  CHECK(decision->approve);
  CHECK(decision->id == "ABC234");
  CHECK_FALSE(caw::Messages::readProposalDecision(R"({"id":"ABC234","approve":"yes"})"));
  CHECK_FALSE(caw::Messages::readProposalDecision(R"({"id":"ABC234","approve":true,"token":"x"})"));
  CHECK_FALSE(caw::Messages::readProposalDecision(R"({"id":"wrong","approve":false})"));
}
