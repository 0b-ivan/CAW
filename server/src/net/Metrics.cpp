// Prometheus text exposition, written by hand: a few lines are clearer than a dependency.
#include "Metrics.h"

#include <unistd.h>

#include <fstream>

namespace caw {
namespace {
void line(std::string& out, const char* name, const char* type, const char* help,
          std::uint64_t value) {
  out += std::string("# HELP ") + name + " " + help + "\n# TYPE " + name + " " + type + "\n";
  out += std::string(name) + " " + std::to_string(value) + "\n";
}

// Resident memory of this process from /proc; 0 where that does not exist.
std::uint64_t residentBytes() {
  std::ifstream statm("/proc/self/statm");
  std::uint64_t size = 0, resident = 0;
  if (!(statm >> size >> resident)) return 0;
  return resident * static_cast<std::uint64_t>(sysconf(_SC_PAGESIZE));
}
}  // namespace

void Metrics::observeAction(double seconds) {
  ++actions_;
  actionSeconds_ += seconds;
  for (std::size_t i = 0; i < kBucketCount; ++i) {
    if (seconds <= kBuckets[i]) {
      ++buckets_[i];
      return;
    }
  }
}

void Metrics::observeFrame(std::size_t bytes) {
  ++frames_;
  frameBytes_ += bytes;
  if (bytes > largestFrame_) largestFrame_ = bytes;
}

void Metrics::observeTick(double seconds) {
  if (seconds > longestTick_) longestTick_ = seconds;
}

std::string Metrics::render() const {
  std::string out;
  line(out, "caw_sessions", "gauge", "Current sessions.", sessions);
  line(out, "caw_active_games", "gauge", "Unfinished matches.", activeGames);
  line(out, "caw_sockets", "gauge", "Open game and big screen sockets.", sockets);
  line(out, "caw_actions_total", "counter", "Game messages handled.", actions_);
  out += "# HELP caw_action_seconds From a game message to all its frames sent.\n";
  out += "# TYPE caw_action_seconds histogram\n";
  std::uint64_t cumulative = 0;
  for (std::size_t i = 0; i < kBucketCount; ++i) {
    cumulative += buckets_[i];
    out += "caw_action_seconds_bucket{le=\"" + std::string(kBucketLabels[i]) + "\"} " +
           std::to_string(cumulative) + "\n";
  }
  out += "caw_action_seconds_bucket{le=\"+Inf\"} " + std::to_string(actions_) + "\n";
  out += "caw_action_seconds_sum " + std::to_string(actionSeconds_) + "\n";
  out += "caw_action_seconds_count " + std::to_string(actions_) + "\n";
  line(out, "caw_frame_bytes_max", "gauge", "Largest frame sent since start.", largestFrame_);
  line(out, "caw_frames_total", "counter", "Frames sent.", frames_);
  line(out, "caw_frame_bytes_total", "counter", "Bytes of all frames sent.", frameBytes_);
  out += "# HELP caw_tick_seconds_max Longest clock run over all rooms since start.\n";
  out += "# TYPE caw_tick_seconds_max gauge\ncaw_tick_seconds_max " + std::to_string(longestTick_) +
         "\n";
  line(out, "caw_resident_bytes", "gauge", "Resident memory of the server process.",
       residentBytes());
  line(out, "caw_rate_limited_total", "counter", "Requests refused by the rate limit.",
       rateLimited);
  line(out, "caw_health_checks_total", "counter", "Readiness probes.", healthChecks);
  line(out, "caw_health_failures_total", "counter", "Failed readiness probes.", healthFailures);
  return out;
}
}  // namespace caw
