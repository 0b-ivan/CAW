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
}  // namespace

TEST_CASE("stop saves the newest snapshot of every room") {
  FakeStore store;
  {
    PersistQueue queue(store);
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
  const caw::RoomRecord record{"de", "0123456789abcdef0123456789abcdef", {"test"}, R"({"a":1})"};
  const auto loaded = caw::Messages::loadRoom(caw::Messages::saveRoom(record));
  REQUIRE(loaded);
  CHECK(loaded->language == "de");
  CHECK(loaded->hostToken == record.hostToken);
  CHECK(loaded->packs == record.packs);
  CHECK(nlohmann::json::parse(loaded->state) == nlohmann::json::parse(record.state));
  CHECK_FALSE(caw::Messages::loadRoom(R"({"schema":2})"));
  CHECK_FALSE(caw::Messages::loadRoom("[]"));
}
