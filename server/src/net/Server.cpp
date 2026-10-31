// REST: create a room and read public lobby metadata. WebSocket: play in a room.
#include "Server.h"

#include <libusockets.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "log.h"
#include "net/Messages.h"
#include "net/Protocol.h"
#include "session/SessionRegistry.h"
#include "store/PersistQueue.h"

namespace caw {
namespace {
struct RequestBody {
  std::string content;
  bool tooLarge = false;
};

void jsonReply(uWS::HttpResponse<false>* response, std::string_view status,
               const std::string& payload) {
  response->writeStatus(status)
      ->writeHeader("Content-Type", "application/json; charset=utf-8")
      ->writeHeader("Cache-Control", "no-store")
      ->end(payload);
}

void createRoom(uWS::HttpResponse<false>* response, SessionRegistry& sessions,
                PersistQueue& persist) {
  auto request = std::make_shared<RequestBody>();
  // onAborted keeps the request buffer alive until the client disconnects.
  response->onAborted([request]() { request->content.clear(); });
  response->onData([response, &sessions, &persist, request](std::string_view chunk, bool final) {
    if (!request->tooLarge) {
      if (chunk.size() > 1024 - request->content.size())
        request->tooLarge = true;
      else
        request->content.append(chunk);
    }
    if (!final) return;
    if (request->tooLarge) {
      jsonReply(response, "413 Payload Too Large", Messages::errorResponse("body_too_large"));
      return;
    }
    const auto language = Messages::createLanguage(request->content);
    if (!language) {
      jsonReply(response, "400 Bad Request", Messages::errorResponse("invalid_request"));
      return;
    }
    try {
      const CreatedSession room = sessions.create(*language);
      // Saved at once, so a restart right after creating keeps the room.
      persist.push(room.code, sessions.snapshot(room.code));
      jsonReply(response, "201 Created", Messages::sessionCreated(room.code, room.hostToken));
    } catch (const std::exception&) {
      // Never emit secrets, connection strings, or library errors to browsers.
      jsonReply(response, "503 Service Unavailable", Messages::errorResponse("room_unavailable"));
    }
  });
}
}  // namespace

Server::Server(SessionRegistry& sessions, const PackLibrary& packs, const FameCache& fame,
               PersistQueue& persist, Metrics& metrics, double requestsPerMinute,
               const std::atomic<bool>& stop)
    : sessions_(sessions),
      packs_(packs),
      fame_(fame),
      persist_(persist),
      metrics_(metrics),
      createLimit_(requestsPerMinute / 10, 5),
      roomLimit_(requestsPerMinute, requestsPerMinute / 2),
      stop_(stop) {}

// Caddy is the only way in and sets X-Forwarded-For; the server port is not published.
bool Server::admitted(RateLimiter& limiter, uWS::HttpResponse<false>* response,
                      uWS::HttpRequest* request) {
  std::string client(request->getHeader("x-forwarded-for"));
  client = client.substr(0, client.find(','));
  if (client.empty()) client = std::string(response->getRemoteAddressAsText());
  if (limiter.allow(client, millisNow())) return true;
  ++metrics_.rateLimited;
  jsonReply(response, "429 Too Many Requests", Messages::errorResponse("rate_limited"));
  return false;
}

void Server::registerRoutes(uWS::App& app) {
  registerSessionRoutes(app);
  registerGameSocket(app, false);
  registerGameSocket(app, true);
}

void Server::registerSessionRoutes(uWS::App& app) {
  app.get("/api/fame", [this](auto* response, auto*) {
    const std::string fame = fame_.get();
    jsonReply(response, "200 OK", fame.empty() ? Messages::fame({}, {}) : fame);
  });
  app.get("/api/packs", [this](auto* response, auto*) {
    jsonReply(response, "200 OK", Messages::packCatalog(packs_.packs()));
  });
  auto createHandler = [this](auto* response, auto* request) {
    if (admitted(createLimit_, response, request)) createRoom(response, sessions_, persist_);
  };
  app.post("/api/sessions", createHandler);
  app.get("/api/sessions/:code", [this](auto* response, auto* request) {
    if (!admitted(roomLimit_, response, request)) return;
    const std::string_view code = request->getParameter(0);
    const auto language = sessions_.language(code);
    if (!language) {
      jsonReply(response, "404 Not Found", Messages::errorResponse("room_not_found"));
      return;
    }
    jsonReply(response, "200 OK", Messages::publicRoom(code, *language));
  });
}

void Server::registerGameSocket(uWS::App& app, bool screen) {
  uWS::App::WebSocketBehavior<SocketData> behavior;
  behavior.maxPayloadLength = Protocol::kMaxMessageBytes;
  behavior.idleTimeout = 60;
  // Above this, uWebSockets drops frames for that socket; it is resynced on drain.
  behavior.maxBackpressure = 64 * 1024;
  behavior.upgrade = [this, screen](auto* response, auto* request, auto* context) {
    if (!admitted(roomLimit_, response, request)) return;
    const std::string code(request->getParameter(0));
    if (sessions_.find(code) == nullptr) {
      jsonReply(response, "404 Not Found", Messages::errorResponse("room_not_found"));
      return;
    }
    response->template upgrade<SocketData>(SocketData{code, nextId_++, false, screen},
                                           request->getHeader("sec-websocket-key"),
                                           request->getHeader("sec-websocket-protocol"),
                                           request->getHeader("sec-websocket-extensions"), context);
  };
  behavior.open = [this](Socket* socket) {
    SocketData& data = *socket->getUserData();
    sockets_[data.id] = socket;
    metrics_.sockets = sockets_.size();
    if (!data.screen) return;
    Session* session = sessions_.find(data.code);
    if (session == nullptr) return;
    session->addScreen(data.id, millisNow());
    deliver(data.code, *session);
  };
  behavior.message = [this](Socket* socket, std::string_view text, uWS::OpCode) {
    onMessage(socket, text);
  };
  behavior.drain = [this](Socket* socket) { onDrain(socket); };
  behavior.close = [this](Socket* socket, int, std::string_view) { onClose(socket); };
  app.ws<SocketData>(screen ? "/ws/:code/screen" : "/ws/:code", std::move(behavior));
}

void Server::onMessage(Socket* socket, std::string_view text) {
  SocketData& data = *socket->getUserData();
  Session* session = sessions_.find(data.code);
  if (session == nullptr) {
    socket->end(1000, "room_closed");
    return;
  }
  if (data.screen) {
    socket->send(Protocol::error("screen_read_only"), uWS::OpCode::TEXT);
    return;
  }
  const auto command = Protocol::parse(text);
  if (!command) {
    socket->send(Protocol::error("invalid_message"), uWS::OpCode::TEXT);
    return;
  }
  const auto started = std::chrono::steady_clock::now();
  session->handle(data.id, *command, millisNow());
  deliver(data.code, *session);
  metrics_.observeAction(
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
}

void Server::onDrain(Socket* socket) {
  SocketData& data = *socket->getUserData();
  if (!data.needsResync || socket->getBufferedAmount() > 0) return;
  Session* session = sessions_.find(data.code);
  if (session == nullptr) return;
  data.needsResync = false;
  session->resync(data.id, millisNow());
  deliver(data.code, *session);
}

void Server::onClose(Socket* socket) {
  const SocketData data = *socket->getUserData();
  sockets_.erase(data.id);
  metrics_.sockets = sockets_.size();
  Session* session = sessions_.find(data.code);
  if (session == nullptr) return;
  session->connectionClosed(data.id, millisNow());
  deliver(data.code, *session);
}

void Server::deliver(const std::string& code, Session& session) {
  for (const Outgoing& frame : session.takeOutgoing()) {
    const auto found = sockets_.find(frame.connection);
    if (found == sockets_.end()) continue;
    Socket* socket = found->second;
    metrics_.observeFrame(frame.text.size());
    if (socket->send(frame.text, uWS::OpCode::TEXT) == Socket::SendStatus::DROPPED)
      socket->getUserData()->needsResync = true;
    if (frame.close) socket->end(1000, "closed");
  }
  if (session.takeChanged() && dirty_.insert(code).second) dirtyOrder_.push_back(code);
  persist_.pushHistory(session.takeHistory());
}

void Server::persistDirty(bool all) {
  const auto deadline = std::chrono::steady_clock::now() + kSnapshotBudget;
  while (!dirtyOrder_.empty() && (all || std::chrono::steady_clock::now() < deadline)) {
    const std::string code = std::move(dirtyOrder_.front());
    dirtyOrder_.pop_front();
    dirty_.erase(code);
    persist(code);
  }
}

void Server::persist(const std::string& code) {
  std::string snapshot = sessions_.snapshot(code);
  if (!snapshot.empty()) persist_.push(code, std::move(snapshot));
}

void Server::tickAll() {
  if (stop_) return shutdown();
  const auto started = std::chrono::steady_clock::now();
  sessions_.expire();
  const std::uint32_t now = millisNow();
  for (const auto& [code, session] : sessions_.all()) {
    session->tick(now);
    deliver(code, *session);
  }
  persistDirty(false);
  metrics_.observeTick(
      std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count());
}

bool Server::start(uWS::App& app, int port) {
  app_ = &app;
  app.listen(port, [this](auto* socket) { listenSocket_ = socket; });
  if (listenSocket_ == nullptr) return false;
  auto* loop = reinterpret_cast<struct us_loop_t*>(uWS::Loop::get());
  timer_ = us_create_timer(loop, 0, sizeof(Server*));
  *static_cast<Server**>(us_timer_ext(timer_)) = this;
  us_timer_set(timer_, &Server::onTimer, kTickMs, kTickMs);
  return true;
}

void Server::shutdown() {
  log::info("Shutting down: closing sockets, saving rooms");
  persistDirty(true);
  us_timer_close(timer_);
  us_listen_socket_close(0, listenSocket_);
  // Phones reconnect with their tickets once the server is back.
  app_->close();
}

void Server::onTimer(struct us_timer_t* timer) {
  (*static_cast<Server**>(us_timer_ext(timer)))->tickAll();
}

}  // namespace caw
