// Runs on the persistence thread only; the game thread never waits for Redis.
#include "RedisStore.h"

#include <iterator>
#include <unordered_set>

#include "log.h"

namespace caw {
namespace {
constexpr const char* kPrefix = "session:";

std::string withTimeouts(const std::string& url) {
  const char* options = "connect_timeout=1000ms&socket_timeout=1000ms";
  return url + (url.find('?') == std::string::npos ? "?" : "&") + options;
}
}  // namespace

RedisStore::RedisStore(const std::string& url, std::chrono::hours ttl)
    : redis_(withTimeouts(url)), ttl_(ttl) {}

bool RedisStore::save(const std::string& code, const std::string& snapshot) {
  try {
    redis_.set(kPrefix + code, snapshot, ttl_);
    return true;
  } catch (const sw::redis::Error&) {
    log::warn("Could not save a room snapshot to Redis");
    return false;
  }
}

std::vector<std::pair<std::string, std::string>> RedisStore::loadAll() {
  std::vector<std::pair<std::string, std::string>> rooms;
  try {
    // SCAN may return a key twice; the set keeps each room once.
    std::unordered_set<std::string> keys;
    sw::redis::Cursor cursor = 0;
    do {
      cursor =
          redis_.scan(cursor, std::string(kPrefix) + "*", 100, std::inserter(keys, keys.end()));
    } while (cursor != 0);
    for (const std::string& key : keys) {
      const auto value = redis_.get(key);
      if (value) rooms.emplace_back(key.substr(std::string(kPrefix).size()), *value);
    }
  } catch (const sw::redis::Error&) {
    log::error("Could not read room snapshots from Redis; starting without them");
    rooms.clear();
  }
  return rooms;
}

}  // namespace caw
