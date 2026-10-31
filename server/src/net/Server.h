// HTTP routes and the game WebSocket on the single uWebSockets event-loop thread.
#pragma once

#include <App.h>

#include <atomic>
#include <string>
#include <unordered_map>

#include "session/Session.h"

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

  // `stop` is set from the SIGTERM handler; the clock notices it and shuts down.
  Server(SessionRegistry& sessions, PersistQueue& persist, const std::atomic<bool>& stop);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void registerRoutes(uWS::App& app);
  // Listens on the port and starts the clock. False if the port cannot be bound.
  bool start(uWS::App& app, int port);

 private:
  void registerSessionRoutes(uWS::App& app);
  // `screen` selects the big-screen route /ws/{code}/screen instead of /ws/{code}.
  void registerGameSocket(uWS::App& app, bool screen);
  void onMessage(Socket* socket, std::string_view text);
  void onDrain(Socket* socket);
  void onClose(Socket* socket);
  void tickAll();
  // Sends what a session produced to the sockets it addressed, then queues a
  // snapshot if anything changed.
  void deliver(const std::string& code, Session& session);
  void persist(const std::string& code);
  // Stops accepting, closes every socket and ends the event loop.
  void shutdown();
  static void onTimer(struct us_timer_t* timer);

  SessionRegistry& sessions_;
  PersistQueue& persist_;
  const std::atomic<bool>& stop_;
  uWS::App* app_ = nullptr;
  struct us_listen_socket_t* listenSocket_ = nullptr;
  struct us_timer_t* timer_ = nullptr;
  std::unordered_map<ConnectionId, Socket*> sockets_;
  ConnectionId nextId_ = 1;
};

}  // namespace caw
