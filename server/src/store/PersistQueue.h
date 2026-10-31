// Hands room snapshots from the game thread to one background thread that saves them.
#pragma once

#include <condition_variable>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "store/SnapshotStore.h"

namespace caw {

class PersistQueue final {
 public:
  explicit PersistQueue(SnapshotStore& store);
  ~PersistQueue();
  PersistQueue(const PersistQueue&) = delete;
  PersistQueue& operator=(const PersistQueue&) = delete;

  // Never blocks on the store. A newer snapshot of the same room replaces a waiting one.
  void push(const std::string& code, std::string snapshot);
  // Saves everything still waiting, then ends the thread. Safe to call twice.
  void stop();

 private:
  void run();

  SnapshotStore& store_;
  std::mutex mutex_;
  std::condition_variable wake_;
  std::map<std::string, std::string> waiting_;  // code → newest snapshot
  bool stopping_ = false;
  std::thread thread_;
};

}  // namespace caw
