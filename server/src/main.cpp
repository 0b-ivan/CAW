// Keep the main event loop small while M2 routes live in net/Server.
#include <App.h>

#include "config.h"
#include "log.h"
#include "net/Metrics.h"
#include "net/Server.h"
#include "session/SessionRegistry.h"
#include "store/Health.h"

int main() {
  try {
    const caw::Config config = caw::readConfig();
    caw::log::setLevel(config.logLevel);
    caw::Metrics metrics;
    caw::SessionRegistry sessions(config.sessionTtlHours);
    bool listening = false;
    uWS::App app;
    app.get("/healthz", [&](auto* response, auto*) {
      ++metrics.healthChecks;
      const bool ready = caw::healthy(config);
      if (!ready) ++metrics.healthFailures;
      response->writeStatus(ready ? "200 OK" : "503 Service Unavailable")
          ->writeHeader("Content-Type", "text/plain; charset=utf-8")
          ->writeHeader("Cache-Control", "no-store")
          ->end(ready ? "ok\n" : "unavailable\n");
    });
    caw::Server::registerSessionRoutes(app, sessions);
    app.get("/metrics", [&](auto* response, auto*) {
      metrics.sessions = sessions.activeCount();
      response->writeHeader("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
          ->end(metrics.render());
    });
    app.listen(config.port, [&](auto* socket) {
      listening = socket != nullptr;
      if (listening) caw::log::info("HTTP server listening");
    });
    if (!listening) {
      caw::log::error("Could not bind HTTP port");
      return 1;
    }
    app.run();
  } catch (const std::exception&) {
    // Library errors can contain credentials; report only this fixed message.
    caw::log::error("Server startup failed; check environment configuration");
    return 1;
  }
}
