// Where room snapshots go. An interface only so tests can replace Redis with a fake.
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace caw {

class SnapshotStore {
 public:
  virtual ~SnapshotStore() = default;
  // Saves or replaces the snapshot of one room. False if the store is unavailable.
  virtual bool save(const std::string& code, const std::string& snapshot) = 0;
  // Every saved room as (code, snapshot), for the start of the server.
  virtual std::vector<std::pair<std::string, std::string>> loadAll() = 0;
};

}  // namespace caw
