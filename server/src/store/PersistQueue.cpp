// The only state shared between threads is `waiting_`, always under `mutex_`.
#include "PersistQueue.h"

#include <utility>

namespace caw {

PersistQueue::PersistQueue(SnapshotStore& store) : store_(store) {
  // Started last, after every member it uses exists.
  thread_ = std::thread([this] { run(); });
}

PersistQueue::~PersistQueue() { stop(); }

void PersistQueue::push(const std::string& code, std::string snapshot) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    waiting_[code] = std::move(snapshot);
  }
  wake_.notify_one();
}

void PersistQueue::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  wake_.notify_one();
  if (thread_.joinable()) thread_.join();
}

void PersistQueue::run() {
  while (true) {
    std::map<std::string, std::string> batch;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait(lock, [this] { return stopping_ || !waiting_.empty(); });
      if (waiting_.empty()) return;  // stopping, and nothing left to save
      batch.swap(waiting_);
    }
    // Saved outside the lock, so the game thread can keep pushing meanwhile.
    for (const auto& [code, snapshot] : batch) store_.save(code, snapshot);
  }
}

}  // namespace caw
