// Wire configuration, persistence, routes and the clock, then run the single event loop.
#include <App.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <string>

#include "config.h"
#include "log.h"
#include "net/Metrics.h"
#include "net/Server.h"
#include "session/SessionRegistry.h"
#include "store/Health.h"
#include "store/PersistQueue.h"
#include "store/RedisStore.h"

namespace {
// Set by SIGTERM/SIGINT; the server's clock checks it on the event-loop thread.
std::atomic<bool> stopRequested{false};
void onStopSignal(int) { stopRequested = true; }

void restoreRooms(caw::SnapshotStore& store, caw::SessionRegistry& sessions) {
  unsigned restored = 0, skipped = 0;
  for (const auto& [code, snapshot] : store.loadAll()) {
    if (sessions.restore(code, snapshot))
      ++restored;
    else
      ++skipped;
  }
  caw::log::info("Restored " + std::to_string(restored) + " rooms from Redis, skipped " +
                 std::to_string(skipped));
}
}  // namespace

int main() {
  try {
    const caw::Config config = caw::readConfig();
    caw::log::setLevel(config.logLevel);
    caw::Metrics metrics;
    caw::SessionRegistry sessions(config.sessionTtlHours);
    caw::RedisStore redis(config.redisUrl, std::chrono::hours(config.sessionTtlHours));
    restoreRooms(redis, sessions);
    caw::PersistQueue persist(redis);
    std::signal(SIGTERM, onStopSignal);
    std::signal(SIGINT, onStopSignal);

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
    caw::Server server(sessions, persist, stopRequested);
    server.registerRoutes(app);
    app.get("/metrics", [&](auto* response, auto*) {
      metrics.sessions = sessions.activeCount();
      response->writeHeader("Content-Type", "text/plain; version=0.0.4; charset=utf-8")
          ->end(metrics.render());
    });
    if (!server.start(app, config.port)) {
      caw::log::error("Could not bind HTTP port");
      return 1;
    }
    caw::log::info("HTTP server listening");
    app.run();
    // The loop ends after shutdown(); save what is still waiting before exiting.
    persist.stop();
    caw::log::info("Stopped");
  } catch (const std::exception&) {
    // Library errors can contain credentials; report only this fixed message.
    caw::log::error("Server startup failed; check environment configuration");
    return 1;
  }
}
