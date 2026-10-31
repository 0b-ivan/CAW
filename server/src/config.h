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
};
Config readConfig();
}  // namespace caw
