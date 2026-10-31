// Prometheus text exposition; gameplay counters stay zero until M2 exists.
#include "Metrics.h"

namespace caw {
std::string Metrics::render() const {
  return "# HELP caw_sessions Current sessions.\n# TYPE caw_sessions gauge\ncaw_sessions 0\n"
         "# HELP caw_sockets Current game sockets.\n# TYPE caw_sockets gauge\ncaw_sockets 0\n"
         "# HELP caw_actions_total Accepted game actions.\n# TYPE caw_actions_total counter\n"
         "caw_actions_total 0\n"
         "# HELP caw_action_seconds Game action latency.\n# TYPE caw_action_seconds histogram\n"
         "caw_action_seconds_bucket{le=\"0.001\"} 0\n"
         "caw_action_seconds_bucket{le=\"0.01\"} 0\n"
         "caw_action_seconds_bucket{le=\"0.05\"} 0\n"
         "caw_action_seconds_bucket{le=\"+Inf\"} 0\n"
         "caw_action_seconds_sum 0\ncaw_action_seconds_count 0\n"
         "# HELP caw_health_checks_total Readiness probes.\n"
         "# TYPE caw_health_checks_total counter\ncaw_health_checks_total " +
         std::to_string(healthChecks) +
         "\n# HELP caw_health_failures_total Failed readiness probes.\n"
         "# TYPE caw_health_failures_total counter\ncaw_health_failures_total " +
         std::to_string(healthFailures) + "\n";
}
}  // namespace caw
