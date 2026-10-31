// Bind M2 HTTP session routes to the single-threaded uWebSockets application.
#pragma once

#include <App.h>

namespace caw {
class SessionRegistry;

class Server final {
 public:
  static void registerSessionRoutes(uWS::App& app, SessionRegistry& sessions);
};
}  // namespace caw
