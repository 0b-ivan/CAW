// M2 session HTTP API: create a room and read only public lobby metadata.
#include "Server.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

#include "net/Messages.h"
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

void Server::registerSessionRoutes(uWS::App& app, SessionRegistry& sessions) {
  app.post("/api/sessions", [&sessions](auto* response, auto*) {
    createRoom(response, sessions);
  });
  app.get("/api/sessions/:code", [&sessions](auto* response, auto* request) {
    const std::string_view code = request->getParameter(0);
    const auto language = sessions.language(code);
    if (!language) {
      jsonReply(response, "404 Not Found", Messages::errorResponse("room_not_found"));
      return;
    }
    jsonReply(response, "200 OK", Messages::publicRoom(code, *language));
  });
}
}  // namespace caw
