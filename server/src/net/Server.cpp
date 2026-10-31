// REST: create a room and read public lobby metadata. WebSocket: play in a room.
#include "Server.h"

#include <libusockets.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "net/Messages.h"
#include "net/Protocol.h"
#include "session/SessionRegistry.h"

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

void createRoom(uWS::HttpResponse<false>* response, SessionRegistry& sessions) {
  auto request = std::make_shared<RequestBody>();
  // onAborted keeps the request buffer alive until the client disconnects.
  response->onAborted([request]() { request->content.clear(); });
  response->onData([response, &sessions, request](std::string_view chunk, bool final) {
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
      jsonReply(response, "201 Created", Messages::sessionCreated(room.code, room.hostToken));
    } catch (const std::exception&) {
      // Never emit secrets, connection strings, or library errors to browsers.
      jsonReply(response, "503 Service Unavailable", Messages::errorResponse("room_unavailable"));
    }
  });
}
}  // namespace

Server::Server(SessionRegistry& sessions) : sessions_(sessions) {}

void Server::registerRoutes(uWS::App& app) {
  registerSessionRoutes(app);
  registerGameSocket(app);
}

void Server::registerSessionRoutes(uWS::App& app) {
  auto createHandler = [this](auto* response, auto*) { createRoom(response, sessions_); };
  app.post("/api/sessions", createHandler);
  app.get("/api/sessions/:code", [this](auto* response, auto* request) {
    const std::string_view code = request->getParameter(0);
    const auto language = sessions_.language(code);
    if (!language) {
      jsonReply(response, "404 Not Found", Messages::errorResponse("room_not_found"));
      return;
    }
    jsonReply(response, "200 OK", Messages::publicRoom(code, *language));
  });
}

void Server::registerGameSocket(uWS::App& app) {
  uWS::App::WebSocketBehavior<SocketData> behavior;
  behavior.maxPayloadLength = Protocol::kMaxMessageBytes;
  behavior.idleTimeout = 60;
  // Above this, uWebSockets drops frames for that socket; it is resynced on drain.
  behavior.maxBackpressure = 64 * 1024;
  behavior.upgrade = [this](auto* response, auto* request, auto* context) {
    const std::string code(request->getParameter(0));
    if (sessions_.find(code) == nullptr) {
      jsonReply(response, "404 Not Found", Messages::errorResponse("room_not_found"));
      return;
    }
    response->template upgrade<SocketData>(SocketData{code, nextId_++, false},
                                           request->getHeader("sec-websocket-key"),
                                           request->getHeader("sec-websocket-protocol"),
                                           request->getHeader("sec-websocket-extensions"), context);
  };
  behavior.open = [this](Socket* socket) { sockets_[socket->getUserData()->id] = socket; };
  behavior.message = [this](Socket* socket, std::string_view text, uWS::OpCode) {
    onMessage(socket, text);
  };
  behavior.drain = [this](Socket* socket) { onDrain(socket); };
  behavior.close = [this](Socket* socket, int, std::string_view) { onClose(socket); };
  app.ws<SocketData>("/ws/:code", std::move(behavior));
}

void Server::onMessage(Socket* socket, std::string_view text) {
  SocketData& data = *socket->getUserData();
  Session* session = sessions_.find(data.code);
  if (session == nullptr) {
    socket->end(1000, "room_closed");
    return;
  }
  const auto command = Protocol::parse(text);
  if (!command) {
    socket->send(Protocol::error("invalid_message"), uWS::OpCode::TEXT);
    return;
  }
  session->handle(data.id, *command, millisNow());
  deliver(*session);
}

void Server::onDrain(Socket* socket) {
  SocketData& data = *socket->getUserData();
  if (!data.needsResync || socket->getBufferedAmount() > 0) return;
  Session* session = sessions_.find(data.code);
  if (session == nullptr) return;
  data.needsResync = false;
  session->resync(data.id, millisNow());
  deliver(*session);
}

void Server::onClose(Socket* socket) {
  const SocketData data = *socket->getUserData();
  sockets_.erase(data.id);
  Session* session = sessions_.find(data.code);
  if (session == nullptr) return;
  session->connectionClosed(data.id, millisNow());
  deliver(*session);
}

void Server::deliver(Session& session) {
  for (const Outgoing& frame : session.takeOutgoing()) {
    const auto found = sockets_.find(frame.connection);
    if (found == sockets_.end()) continue;
    Socket* socket = found->second;
    if (socket->send(frame.text, uWS::OpCode::TEXT) == Socket::SendStatus::DROPPED)
      socket->getUserData()->needsResync = true;
    if (frame.close) socket->end(1000, "closed");
  }
}

void Server::tickAll() {
  sessions_.expire();
  const std::uint32_t now = millisNow();
  for (Session* session : sessions_.all()) {
    session->tick(now);
    deliver(*session);
  }
}

void Server::startClock() {
  auto* loop = reinterpret_cast<struct us_loop_t*>(uWS::Loop::get());
  struct us_timer_t* timer = us_create_timer(loop, 0, sizeof(Server*));
  *static_cast<Server**>(us_timer_ext(timer)) = this;
  us_timer_set(timer, &Server::onTimer, kTickMs, kTickMs);
}

void Server::onTimer(struct us_timer_t* timer) {
  (*static_cast<Server**>(us_timer_ext(timer)))->tickAll();
}

}  // namespace caw
