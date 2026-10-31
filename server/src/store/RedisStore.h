// Room snapshots in Redis under "session:{code}", expiring with the room.
#pragma once

#include <sw/redis++/redis++.h>

#include <chrono>
#include <string>

#include "store/SnapshotStore.h"

namespace caw {

class RedisStore final : public SnapshotStore {
 public:
  RedisStore(const std::string& url, std::chrono::hours ttl);

  bool save(const std::string& code, const std::string& snapshot) override;
  std::vector<std::pair<std::string, std::string>> loadAll() override;

 private:
  sw::redis::Redis redis_;
  std::chrono::seconds ttl_;
};

}  // namespace caw
