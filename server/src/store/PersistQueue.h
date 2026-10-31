// Hands room snapshots and statistics from the game thread to one background thread
// that saves them: Redis for the snapshots, Postgres for the statistics.
#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "store/HistoryStore.h"
#include "store/SnapshotStore.h"

namespace caw {

class PersistQueue final {
 public:
  PersistQueue(SnapshotStore& store, HistoryStore& history);
  ~PersistQueue();
  PersistQueue(const PersistQueue&) = delete;
  PersistQueue& operator=(const PersistQueue&) = delete;

  // Never blocks on the store. A newer snapshot of the same room replaces a waiting one.
  void push(const std::string& code, std::string snapshot);
  // Statistics are never replaced, only appended.
  void pushHistory(std::vector<HistoryEvent> events);
  // Saves everything still waiting, then ends the thread. Safe to call twice.
  void stop();

 private:
  void run();

  SnapshotStore& store_;
  HistoryStore& history_;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::map<std::string, std::string> waiting_;  // code → newest snapshot
  std::vector<HistoryEvent> events_;
  bool stopping_ = false;
  std::thread thread_;
};

}  // namespace caw
