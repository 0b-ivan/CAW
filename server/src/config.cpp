// Validate configuration at startup without disclosing connection strings.
#include "config.h"

#include <charconv>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace caw {
namespace {
std::string environment(const char* name, const char* fallback = "") {
  const char* value = std::getenv(name);
  return value ? value : fallback;
}
unsigned number(const char* name, const char* fallback, unsigned maximum) {
  const std::string text = environment(name, fallback);
  unsigned value = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value == 0 ||
      value > maximum)
    throw std::runtime_error(std::string("Invalid ") + name);
  return value;
}
void requireUrl(const std::string& value, std::string_view prefix, const char* name) {
  if (!value.starts_with(prefix) || value.size() == prefix.size())
    throw std::runtime_error(std::string("Invalid ") + name);
}
}  // namespace
Config readConfig() {
  Config config;
  config.port = static_cast<int>(number("PORT", "8080", 65535));
  config.sessionTtlHours = number("SESSION_TTL_HOURS", "24", 8760);
  config.publicUrl = environment("PUBLIC_URL", "http://localhost:8080");
  config.databaseUrl = environment("DATABASE_URL");
  config.redisUrl = environment("REDIS_URL");
  config.logLevel = environment("LOG_LEVEL", "info");
  config.packsDir = environment("PACKS_DIR", "/app/packs");
  config.rateLimitPerMinute = number("RATE_LIMIT_PER_MINUTE", "60", 10000000);
  if (!config.publicUrl.starts_with("http://") && !config.publicUrl.starts_with("https://"))
    throw std::runtime_error("Invalid PUBLIC_URL");
  requireUrl(config.databaseUrl, "postgresql://", "DATABASE_URL");
  requireUrl(config.redisUrl, "tcp://", "REDIS_URL");
  if (config.logLevel != "debug" && config.logLevel != "info" && config.logLevel != "warn" &&
      config.logLevel != "error")
    throw std::runtime_error("Invalid LOG_LEVEL");
  return config;
}
}  // namespace caw
