// The only state shared between threads is `waiting_` and `events_`, always under `mutex_`.
#include "PersistQueue.h"

#include <chrono>
#include <utility>

#include "log.h"

namespace caw {

PersistQueue::PersistQueue(SnapshotStore& store, HistoryStore& history)
    : store_(store), history_(history) {
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

void PersistQueue::pushHistory(std::vector<HistoryEvent> events) {
  if (events.empty()) return;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (HistoryEvent& event : events) events_.push_back(std::move(event));
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
  unsigned shutdownFailures = 0;
  while (true) {
    std::map<std::string, std::string> batch;
    std::vector<HistoryEvent> events;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      wake_.wait(lock, [this] { return stopping_ || !waiting_.empty() || !events_.empty(); });
      if (waiting_.empty() && events_.empty()) return;  // stopping, and nothing left to save
      batch.swap(waiting_);
      events.swap(events_);
    }
    // Saved outside the lock, so the game thread can keep pushing meanwhile.
    bool failed = false;
    for (const auto& [code, snapshot] : batch) {
      if (store_.save(code, snapshot)) continue;
      failed = true;
      std::lock_guard<std::mutex> lock(mutex_);
      // Never let a failed older write replace a newer game state.
      waiting_.try_emplace(code, snapshot);
    }
    if (!events.empty()) history_.record(events);
    if (!failed) {
      shutdownFailures = 0;
      continue;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    if (stopping_) {
      // A permanently down Redis must not hold shutdown forever.
      if (++shutdownFailures >= 5) {
        log::error("Room snapshots remain unsaved after shutdown retries");
        waiting_.clear();
      }
    } else {
      // Retry after an outage, but never spin at full speed while Redis is down.
      wake_.wait_for(lock, std::chrono::seconds(1), [this] { return stopping_; });
    }
  }
}

}  // namespace caw
