// Probe on demand with bounded connection and command timeouts.
#include "Health.h"

#include <sw/redis++/redis++.h>

#include <pqxx/pqxx>

#include "log.h"

namespace caw {
namespace {
std::string withOptions(const std::string& url, const char* options) {
  return url + (url.find('?') == std::string::npos ? "?" : "&") + options;
}
bool redisHealthy(const Config& config) {
  try {
    sw::redis::Redis redis(
        withOptions(config.redisUrl, "connect_timeout=500ms&socket_timeout=500ms"));
    return redis.ping() == "PONG";
  } catch (const sw::redis::Error&) {
    log::warn("Redis readiness probe failed");
    return false;
  }
}
bool postgresHealthy(const Config& config) {
  try {
    pqxx::connection connection(withOptions(config.databaseUrl, "connect_timeout=2"));
    pqxx::work transaction(connection);
    transaction.exec("SET LOCAL statement_timeout = '1000ms'");
    return transaction.exec("SELECT 1")[0][0].as<int>() == 1;
  } catch (const std::exception&) {
    log::warn("Postgres readiness probe failed");
    return false;
  }
}
}  // namespace
bool healthy(const Config& config) {
  const bool redis = redisHealthy(config);
  const bool postgres = postgresHealthy(config);
  return redis && postgres;
}
}  // namespace caw
