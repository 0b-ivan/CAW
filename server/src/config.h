// Environment-only process configuration.
#pragma once
#include <string>

namespace caw {
struct Config {
  int port = 8080;
  unsigned sessionTtlHours = 24;
  std::string publicUrl;
  std::string databaseUrl;
  std::string redisUrl;
  std::string logLevel = "info";
  std::string packsDir = "/app/packs";  // one folder per source, each with a packs.json
  unsigned rateLimitPerMinute = 60;     // room lookups and sockets per client; creating: a tenth
};
Config readConfig();
}  // namespace caw
