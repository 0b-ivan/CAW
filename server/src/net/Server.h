// HTTP routes and the game WebSocket on the single uWebSockets event-loop thread.
#pragma once

#include <App.h>

#include <string>
#include <unordered_map>

#include "session/Session.h"

namespace caw {
class SessionRegistry;

// Per-socket state uWebSockets stores for us.
struct SocketData {
  std::string code;
  ConnectionId id = 0;
  bool needsResync = false;  // a frame was dropped under backpressure
};

class Server final {
 public:
  using Socket = uWS::WebSocket<false, true, SocketData>;
  // How often the match clocks run.
  static constexpr int kTickMs = 250;

  explicit Server(SessionRegistry& sessions);
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  void registerRoutes(uWS::App& app);
  // Starts the repeating clock tick on the current event loop.
  void startClock();

 private:
  void registerSessionRoutes(uWS::App& app);
  void registerGameSocket(uWS::App& app);
  void onMessage(Socket* socket, std::string_view text);
  void onDrain(Socket* socket);
  void onClose(Socket* socket);
  void tickAll();
  // Sends what a session produced to the sockets it addressed.
  void deliver(Session& session);
  static void onTimer(struct us_timer_t* timer);

  SessionRegistry& sessions_;
  std::unordered_map<ConnectionId, Socket*> sockets_;
  ConnectionId nextId_ = 1;
};

}  // namespace caw
