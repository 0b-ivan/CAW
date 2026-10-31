// A token bucket per client address: `burst` requests at once, refilled at `perMinute`.
// Owned by the event loop; no locking.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace caw {

class RateLimiter final {
 public:
  RateLimiter(double perMinute, double burst);

  // True if this key may make one more request now (milliseconds, monotonic).
  bool allow(const std::string& key, std::uint32_t now);

 private:
  struct Bucket {
    double tokens = 0;
    std::uint32_t at = 0;
  };
  // Full buckets carry no information; dropping them keeps the map small.
  void forgetFullBuckets(std::uint32_t now);

  double perMillisecond_;
  double burst_;
  std::unordered_map<std::string, Bucket> buckets_;
};

}  // namespace caw
