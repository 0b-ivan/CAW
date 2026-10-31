// Event-loop-owned counters, exported in the Prometheus text format without a library.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace caw {
struct Metrics {
  // Upper bounds of the action latency buckets, in seconds; +Inf is implied.
  static constexpr double kBuckets[] = {0.001, 0.005, 0.01, 0.05};
  static constexpr const char* kBucketLabels[] = {"0.001", "0.005", "0.01", "0.05"};
  static constexpr std::size_t kBucketCount = sizeof(kBuckets) / sizeof(kBuckets[0]);

  std::uint64_t sessions = 0;
  std::uint64_t sockets = 0;
  std::uint64_t healthChecks = 0;
  std::uint64_t healthFailures = 0;
  std::uint64_t rateLimited = 0;

  // Accepted message → every resulting frame handed to the sockets.
  void observeAction(double seconds);
  void observeFrame(std::size_t bytes);
  // One run of the clock over all rooms, including their snapshots: the event loop is blocked
  // that long, so its maximum is the worst extra delay a move can see.
  void observeTick(double seconds);
  std::string render() const;

 private:
  std::uint64_t actions_ = 0;
  double actionSeconds_ = 0;
  std::uint64_t buckets_[kBucketCount] = {};  // not cumulative; render() sums them up
  std::size_t largestFrame_ = 0;
  std::uint64_t frames_ = 0;
  std::uint64_t frameBytes_ = 0;
  double longestTick_ = 0;
};
}  // namespace caw
