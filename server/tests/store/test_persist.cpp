// Snapshots reach the store in the background, newest per room, all of them on stop.
#include <doctest/doctest.h>

#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

#include "TestPacks.h"
#include "net/Messages.h"
#include "session/SessionRegistry.h"
#include "store/PersistQueue.h"

using caw::PersistQueue;
using caw::SessionRegistry;

namespace {
// Records every save; the queue's thread writes, the test thread reads after stop().
struct FakeStore final : caw::SnapshotStore {
  std::mutex mutex;
  std::map<std::string, std::string> latest;
  std::map<std::string, int> saves;
  bool save(const std::string& code, const std::string& snapshot) override {
    std::lock_guard<std::mutex> lock(mutex);
    latest[code] = snapshot;
    ++saves[code];
    return true;
  }
  std::vector<std::pair<std::string, std::string>> loadAll() override {
    return {latest.begin(), latest.end()};
  }
};

struct FakeHistory final : caw::HistoryStore {
  std::mutex mutex;
  std::vector<caw::HistoryEvent> events;
  bool record(const std::vector<caw::HistoryEvent>& batch) override {
    std::lock_guard<std::mutex> lock(mutex);
    events.insert(events.end(), batch.begin(), batch.end());
    return true;
  }
};
}  // namespace

TEST_CASE("statistics are appended, never replaced, and all written on stop") {
  FakeStore store;
  FakeHistory history;
  {
    PersistQueue queue(store, history);
    for (int i = 0; i < 50; ++i)
      queue.pushHistory({{caw::HistoryEvent::Kind::RoundWon, "00000000000000aa", "Ada", 0, false,
                          "Satz " + std::to_string(i)}});
    queue.pushHistory({});
    queue.stop();
  }
  REQUIRE(history.events.size() == 50);
  CHECK(history.events.front().sentence == "Satz 0");
  CHECK(history.events.back().sentence == "Satz 49");
}

TEST_CASE("stop saves the newest snapshot of every room") {
  FakeStore store;
  {
    FakeHistory history;
    PersistQueue queue(store, history);
    for (int i = 0; i < 1000; ++i) queue.push("AAAAAA", std::to_string(i));
    queue.push("BBBBBB", "only");
    queue.stop();
    queue.stop();  // harmless twice
  }
  CHECK(store.latest["AAAAAA"] == "999");
  CHECK(store.latest["BBBBBB"] == "only");
  // Waiting snapshots of one room are replaced, not all written.
  CHECK(store.saves["AAAAAA"] <= 1000);
}

TEST_CASE("a room survives a registry restart through its snapshot") {
  SessionRegistry first(24, testPacks());
  const auto room = first.create("en");
  const std::string snapshot = first.snapshot(room.code);
  REQUIRE_FALSE(snapshot.empty());
  CHECK(first.snapshot("ZZZZZZ").empty());

  SessionRegistry second(24, testPacks());
  REQUIRE(second.restore(room.code, snapshot));
  CHECK(second.language(room.code) == "en");
  CHECK(second.authenticatedHost(room.code, room.hostToken));
  CHECK(second.find(room.code) != nullptr);
  // The same code twice, broken text and invalid codes are refused.
  CHECK_FALSE(second.restore(room.code, snapshot));
  CHECK_FALSE(second.restore("CCCCCC", "{}"));
  CHECK_FALSE(second.restore("lower!", snapshot));
}

TEST_CASE("room records keep language, host token and state together") {
  const caw::RoomRecord record{
      "de",         "0123456789abcdef0123456789abcdef", {"test"},
      R"({"a":1})", {{"00000000000000aa", 7}, {}},      {{"Ada", "Ein ganzer Satz."}},
      true};
  const auto loaded = caw::Messages::loadRoom(caw::Messages::saveRoom(record));
  REQUIRE(loaded);
  CHECK(loaded->language == "de");
  CHECK(loaded->hostToken == record.hostToken);
  CHECK(loaded->packs == record.packs);
  REQUIRE(loaded->identities.size() == 2);
  CHECK(loaded->identities[0].playerId == "00000000000000aa");
  CHECK(loaded->identities[0].word == 7);
  CHECK(loaded->identities[1].playerId.empty());
  REQUIRE(loaded->lines.size() == 1);
  CHECK(loaded->lines[0].sentence == "Ein ganzer Satz.");
  CHECK(loaded->locked);
  CHECK(nlohmann::json::parse(loaded->state) == nlohmann::json::parse(record.state));
  CHECK_FALSE(caw::Messages::loadRoom(R"({"schema":2})"));
  CHECK_FALSE(caw::Messages::loadRoom("[]"));
}

TEST_CASE("a room's own card pack survives Redis restore but stays isolated") {
  caw::PackLibrary packs;
  REQUIRE(packs.add(caw::syntheticPack()));
  SessionRegistry first(24, packs);
  const auto room = first.create("de");
  const auto other = first.create("de");
  auto cards = caw::Messages::readCustomCards(
      R"({"prompts":["Meine Testfrage ___."],"answers":["Meine Testantwort"]})");
  REQUIRE(cards);
  REQUIRE(first.updateCustomCards(room.code, *cards));
  REQUIRE(first.customCards(room.code));
  const std::string previousId = first.customCards(room.code)->id;
  REQUIRE(packs.find(previousId) != nullptr);

  auto strangerChoice = first.find(other.code)->packs();
  strangerChoice.push_back(previousId);
  CHECK_FALSE(first.find(other.code)->activatePacks(strangerChoice, caw::millisNow()));

  cards->answers = {"Neue Testantwort"};
  REQUIRE(first.updateCustomCards(room.code, *cards));
  CHECK(packs.find(previousId) == nullptr);
  REQUIRE(first.customCards(room.code));
  CHECK(first.customCards(room.code)->answers[0] == "Neue Testantwort");
  const std::string saved = first.snapshot(room.code);

  caw::PackLibrary restoredPacks;
  REQUIRE(restoredPacks.add(caw::syntheticPack()));
  SessionRegistry restored(24, restoredPacks);
  REQUIRE(restored.restore(room.code, saved));
  REQUIRE(restored.customCards(room.code));
  CHECK(restored.customCards(room.code)->answers[0] == "Neue Testantwort");
  CHECK(restored.find(room.code)->packs() == first.find(room.code)->packs());
  CHECK_FALSE(restored.customCards(other.code));
}

TEST_CASE("schema 4 room snapshots remain restorable without custom cards") {
  caw::RoomRecord room{"de", "0123456789abcdef0123456789abcdef", {"test"},
                       R"({"state":1})", {}, {}, false};
  auto legacy = nlohmann::json::parse(caw::Messages::saveRoom(room));
  legacy["schema"] = 4;
  legacy.erase("custom");
  const auto decoded = caw::Messages::loadRoom(legacy.dump());
  REQUIRE(decoded);
  CHECK_FALSE(decoded->customPack);
}
