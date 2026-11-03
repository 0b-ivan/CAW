// Invalid deployment configuration must fail without exposing credentials.
#include <doctest/doctest.h>

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "config.h"
#include "log.h"
#include "net/Metrics.h"
#include "net/RateLimiter.h"

namespace {
void defaults() {
  unsetenv("PORT");
  unsetenv("SESSION_TTL_HOURS");
  unsetenv("LOG_LEVEL");
  unsetenv("PUBLIC_URL");
  setenv("DATABASE_URL", "postgresql://caw:test@postgres/caw", 1);
  setenv("REDIS_URL", "tcp://redis:6379", 1);
}
}  // namespace
TEST_CASE("configuration defaults and numeric boundaries") {
  defaults();
  const caw::Config config = caw::readConfig();
  CHECK(config.port == 8080);
  CHECK(config.sessionTtlHours == 24);
  for (const char* value : {"1", "65535"}) {
    setenv("PORT", value, 1);
    CHECK_NOTHROW(caw::readConfig());
  }
  for (const char* value : {"", "0", "-1", "65536", "8080x", " 8080", "99999999999999"}) {
    setenv("PORT", value, 1);
    CHECK_THROWS_WITH(caw::readConfig(), "Invalid PORT");
  }
  defaults();
  setenv("SESSION_TTL_HOURS", "0", 1);
  CHECK_THROWS_WITH(caw::readConfig(), "Invalid SESSION_TTL_HOURS");
  defaults();
  setenv("LOG_LEVEL", "invalid", 1);
  CHECK_THROWS_WITH(caw::readConfig(), "Invalid LOG_LEVEL");
  defaults();
  setenv("DATABASE_URL", "secret-password", 1);
  CHECK_THROWS_WITH(caw::readConfig(), "Invalid DATABASE_URL");
  defaults();
  unsetenv("REDIS_URL");
  CHECK_THROWS_WITH(caw::readConfig(), "Invalid REDIS_URL");
}
TEST_CASE("logger escapes controls and honors configured severity") {
  std::ostringstream captured;
  auto* previous = std::cout.rdbuf(captured.rdbuf());
  caw::log::setLevel("warn");
  caw::log::info("filtered");
  caw::log::warn("quoted \" and slash \\ and newline\n");
  std::cout.rdbuf(previous);
  CHECK(captured.str() ==
        "{\"level\":\"warn\",\"message\":\"quoted \\\" and slash \\\\ and newline\\u000a\"}\n");
  caw::log::setLevel("info");
}
TEST_CASE("metrics count actual readiness requests") {
  caw::Metrics metrics;
  metrics.healthChecks = 3;
  metrics.healthFailures = 1;
  const auto text = metrics.render();
  CHECK(text.find("caw_health_checks_total 3\n") != std::string::npos);
  CHECK(text.find("caw_health_failures_total 1\n") != std::string::npos);
  CHECK(text.find("caw_action_seconds_bucket{le=\"+Inf\"} 0\n") != std::string::npos);
}
TEST_CASE("the action histogram is cumulative and frames keep their maximum") {
  caw::Metrics metrics;
  metrics.observeAction(0.0004);
  metrics.observeAction(0.007);
  metrics.observeAction(0.2);
  metrics.observeFrame(900);
  metrics.observeFrame(300);
  const auto text = metrics.render();
  CHECK(text.find("caw_action_seconds_bucket{le=\"0.001\"} 1\n") != std::string::npos);
  CHECK(text.find("caw_action_seconds_bucket{le=\"0.01\"} 2\n") != std::string::npos);
  CHECK(text.find("caw_action_seconds_bucket{le=\"0.05\"} 2\n") != std::string::npos);
  CHECK(text.find("caw_action_seconds_bucket{le=\"+Inf\"} 3\n") != std::string::npos);
  CHECK(text.find("caw_frame_bytes_max 900\n") != std::string::npos);
}

TEST_CASE("the rate limit allows a burst, then refills over time, per client") {
  caw::RateLimiter limit(60, 3);  // one per second, three at once
  CHECK(limit.allow("a", 0));
  CHECK(limit.allow("a", 0));
  CHECK(limit.allow("a", 0));
  CHECK_FALSE(limit.allow("a", 0));
  CHECK(limit.allow("b", 0));  // another client is not affected
  CHECK_FALSE(limit.allow("a", 500));
  CHECK(limit.allow("a", 1600));
  CHECK_FALSE(limit.allow("a", 1600));
}

TEST_CASE("active game metric is independent from retained room count") {
  caw::Metrics metrics;
  metrics.sessions = 12;
  metrics.activeGames = 2;
  const auto text = metrics.render();
  CHECK(text.find("caw_sessions 12\n") != std::string::npos);
  CHECK(text.find("caw_active_games 2\n") != std::string::npos);
}
