// The Wall of Fame as ready JSON (Messages::fame): written by the persistence thread after
// each save, read by the event loop for GET /api/fame without ever touching the database.
#pragma once

#include <mutex>
#include <string>

namespace caw {

class FameCache final {
 public:
  void set(std::string json) {
    std::lock_guard<std::mutex> lock(mutex_);
    json_ = std::move(json);
  }
  std::string get() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return json_;
  }

 private:
  mutable std::mutex mutex_;
  std::string json_;  // empty until the first read from Postgres
};

}  // namespace caw
