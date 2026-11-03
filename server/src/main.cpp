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
#include "packs/PackLibrary.h"
#include "packs/SyntheticPack.h"
#include "session/SessionRegistry.h"
#include "store/Health.h"
#include "store/PersistQueue.h"
#include "store/PostgresStore.h"
#include "store/RedisStore.h"

namespace {
// Set by SIGTERM/SIGINT; the server's clock checks it on the event-loop thread.
std::atomic<bool> stopRequested{false};
void onStopSignal(int) { stopRequested = true; }

// Without playable packs a room could not even be created: fall back to numbered cards.
void loadPacks(const std::string& directory, caw::PackLibrary& packs) {
  const std::size_t loaded = packs.loadDirectory(directory);
  caw::log::info("Loaded " + std::to_string(loaded) + " card packs from " + directory);
  if (packs.deck(packs.defaultSelection("de")) && packs.deck(packs.defaultSelection("en"))) return;
  caw::log::warn("No playable packs, using numbered test cards");
  packs.add(caw::syntheticPack());
}

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
    caw::PackLibrary packs;
    loadPacks(config.packsDir, packs);
    caw::SessionRegistry sessions(config.sessionTtlHours, packs);
    caw::RedisStore redis(config.redisUrl, std::chrono::hours(config.sessionTtlHours));
    restoreRooms(redis, sessions);
    caw::FameCache fame;
    caw::PostgresStore postgres(config.databaseUrl, fame);
    if (!postgres.prepare())
      caw::log::warn("Starting without statistics; they are kept once Postgres is back");
    caw::PersistQueue persist(redis, postgres);
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
    caw::Server server(sessions, packs, fame, persist, metrics, config.rateLimitPerMinute,
                       stopRequested);
    server.registerRoutes(app);
    app.get("/metrics", [&](auto* response, auto*) {
      metrics.sessions = sessions.activeCount();
      metrics.activeGames = sessions.inProgressCount();
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
