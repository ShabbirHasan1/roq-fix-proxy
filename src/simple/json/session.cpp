/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/json/session.hpp"

#include <utility>

#include <nlohmann/json.hpp>

#include "roq/exceptions.hpp"
#include "roq/logging.hpp"

#include "roq/web/rest/server_factory.hpp"

using namespace std::literals;

namespace simple {
namespace json {

// === IMPLEMENTATION ===

Session::Session(Handler &handler, uint64_t session_id, roq::io::net::tcp::Connection::Factory &factory, Shared &shared)
    : handler_{handler}, session_id_{session_id}, server_{roq::web::rest::ServerFactory::create(*this, factory)},
      shared_{shared} {
}

// web::rest::Server::Handler

void Session::operator()(roq::web::rest::Server::Disconnected const &) {
  shared_.sessions_to_remove.emplace(session_id_);
}

// note!
//   roq::web::rest::Server guarantees that there will always be exactly one response
//   - an automatic "not-found" (404) response is generated if this handler doesn't send anything
//   - an exception is thrown if this handler tries to send more than one response
void Session::operator()(roq::web::rest::Server::Request const &request) {
  auto success = false;
  try {
    if (request.headers.connection == roq::web::http::Connection::UPGRADE) {
      roq::log::info("Upgrading session_id={} to websocket..."sv, session_id_);
      (*server_).upgrade(request);
    } else {
      roq::log::info("DEBUG request={}"sv, request);
      auto path = request.path;  // note! url path has already been split
      if (!std::empty(path) && !std::empty(shared_.settings.json.url_prefix) &&
          path[0] == shared_.settings.json.url_prefix)
        path = path.subspan(1);  // drop prefix
      if (!std::empty(path)) {
        Response response{*server_, request, shared_.encode_buffer};
        route(response, request, path);
      }
    }
    success = true;
  } catch (roq::RuntimeError &e) {
    roq::log::error("Error: {}"sv, e);
  } catch (std::exception &e) {
    roq::log::error("Error: {}"sv, e.what());
  }
  if (!success)
    (*server_).close();
}

void Session::operator()(roq::web::rest::Server::Text const &text) {
  roq::log::info(R"(message="{})"sv, text.payload);
  auto success = false;
  try {
    process(text.payload);
    success = true;
  } catch (roq::RuntimeError &e) {
    roq::log::error("Error: {}"sv, e);
  } catch (std::exception &e) {
    roq::log::error("Error: {}"sv, e.what());
  }
  if (!success)
    (*server_).close();
}

void Session::operator()(roq::web::rest::Server::Binary const &) {
  roq::log::warn("Unexpected"sv);
  (*server_).close();
}

// rest

void Session::route(
    Response &response, roq::web::rest::Server::Request const &request, std::span<std::string_view> const &path) {
  switch (request.method) {
    using enum roq::web::http::Method;
    case GET:
      if (path[0] == "exchanges"sv)
        get_exchanges(response, request);
      else if (path[0] == "symbols"sv)
        get_symbols(response, request);
      break;
    case HEAD:
      break;
    case POST:
      if (path[0] == "order"sv)
        post_order(response, request);
      break;
    case PUT:
      break;
    case DELETE:
      if (path[0] == "order"sv)
        delete_order(response, request);
      break;
    case CONNECT:
      break;
    case OPTIONS:
      break;
    case TRACE:
      break;
  }
}

void Session::get_exchanges(Response &response, roq::web::rest::Server::Request const &) {
  response(
      roq::web::http::Status::NOT_FOUND,
      roq::web::http::ContentType::APPLICATION_JSON,
      R"({{"status":"{}"}})"sv,
      "not implemented"sv);
}

void Session::get_symbols(Response &response, roq::web::rest::Server::Request const &) {
  response(
      roq::web::http::Status::NOT_FOUND,
      roq::web::http::ContentType::APPLICATION_JSON,
      R"({{"status":"{}"}})"sv,
      "not implemented"sv);
}

void Session::post_order(Response &response, roq::web::rest::Server::Request const &) {
  response(
      roq::web::http::Status::NOT_FOUND,
      roq::web::http::ContentType::APPLICATION_JSON,
      R"({{"status":"{}"}})"sv,
      "not implemented"sv);
}

void Session::delete_order(Response &response, roq::web::rest::Server::Request const &) {
  response(
      roq::web::http::Status::NOT_FOUND,
      roq::web::http::ContentType::APPLICATION_JSON,
      R"({{"status":"{}"}})"sv,
      "not implemented"sv);
}

// ws

void Session::process(std::string_view const &payload) {
  // XXX https://www.jsonrpc.org/specification
  (*server_).send_text(payload);  // XXX TODO implement protocol (this will just echo the message)
}

}  // namespace json
}  // namespace simple
