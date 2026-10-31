// Event-loop-owned counters, exported without a metrics dependency.
#pragma once
#include <cstdint>
#include <string>

namespace caw {
struct Metrics {
  std::uint64_t healthChecks = 0;
  std::uint64_t healthFailures = 0;
  std::string render() const;
};
}  // namespace caw
