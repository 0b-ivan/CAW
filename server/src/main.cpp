// Wire the M0 HTTP endpoints; game transport and sessions belong to M2.
#include <App.h>

#include "config.h"
#include "log.h"
#include "net/Metrics.h"
#include "store/Health.h"

int main() {
  try {
    const caw::Config config = caw::readConfig();
    caw::log::setLevel(config.logLevel);
    caw::Metrics metrics;
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
    app.get("/metrics", [&](auto* response, auto*) {
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
