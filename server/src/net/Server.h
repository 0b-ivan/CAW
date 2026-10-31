// HTTP routes and the game WebSocket on the single uWebSockets event-loop thread.
#pragma once

#include <App.h>

#include <atomic>
#include <chrono>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "net/Metrics.h"
#include "net/RateLimiter.h"
#include "session/Session.h"
#include "store/FameCache.h"

namespace caw {
class PersistQueue;
class SessionRegistry;

// Per-socket state uWebSockets stores for us.
struct SocketData {
  std::string code;
  ConnectionId id = 0;
  bool needsResync = false;  // a frame was dropped under backpressure
  bool screen = false;       // a big screen: receives the public table, sends nothing
};

class Server final {
 public:
  using Socket = uWS::WebSocket<false, true, SocketData>;
  // How often the match clocks run.
  static constexpr int kTickMs = 250;
  static constexpr std::chrono::milliseconds kSnapshotBudget{20};

  // `stop` is set from the SIGTERM handler; the clock notices it and shuts down.
  // Requests per minute and client address for joining and looking up rooms; creating a
  // room allows a tenth of it.
  Server(SessionRegistry& sessions, const PackLibrary& packs, const FameCache& fame,
         PersistQueue& persist, Metrics& metrics, double requestsPerMinute,
         const std::atomic<bool>& stop);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void registerRoutes(uWS::App& app);
  // Listens on the port and starts the clock. False if the port cannot be bound.
  bool start(uWS::App& app, int port);

 private:
  void registerSessionRoutes(uWS::App& app);
  // False, after answering 429, if this client has used up its requests on `limiter`.
  bool admitted(RateLimiter& limiter, uWS::HttpResponse<false>* response,
                uWS::HttpRequest* request);
  // `screen` selects the big-screen route /ws/{code}/screen instead of /ws/{code}.
  void registerGameSocket(uWS::App& app, bool screen);
  void onMessage(Socket* socket, std::string_view text);
  void onDrain(Socket* socket);
  void onClose(Socket* socket);
  void tickAll();
  // Sends what a session produced to the sockets it addressed and notes the room for
  // the next snapshot if anything changed.
  void deliver(const std::string& code, Session& session);
  void persist(const std::string& code);
  // Saves changed rooms within the time budget; with `all`, every one of them.
  void persistDirty(bool all);
  // Stops accepting, closes every socket and ends the event loop.
  void shutdown();
  static void onTimer(struct us_timer_t* timer);

  SessionRegistry& sessions_;
  const PackLibrary& packs_;
  const FameCache& fame_;
  PersistQueue& persist_;
  Metrics& metrics_;
  RateLimiter createLimit_;
  RateLimiter roomLimit_;
  const std::atomic<bool>& stop_;
  uWS::App* app_ = nullptr;
  struct us_listen_socket_t* listenSocket_ = nullptr;
  struct us_timer_t* timer_ = nullptr;
  std::unordered_map<ConnectionId, Socket*> sockets_;
  // Rooms changed since their last snapshot, oldest change first. Each tick saves them
  // until kSnapshotBudget is used up, so saving never stalls the game; SIGTERM saves all.
  std::deque<std::string> dirtyOrder_;
  std::unordered_set<std::string> dirty_;
  ConnectionId nextId_ = 1;
};

}  // namespace caw
