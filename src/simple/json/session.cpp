/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/json/session.hpp"

#include <nlohmann/json.hpp>

#include <utility>

#include "roq/exceptions.hpp"
#include "roq/logging.hpp"

#include "roq/web/rest/server_factory.hpp"

using namespace std::literals;

namespace simple {
namespace json {

// === CONSTANTS ===

namespace {
auto const JSONRPC_VERSION = "2.0"sv;
}

// === IMPLEMENTATION ===

Session::Session(Handler &handler, uint64_t session_id, roq::io::net::tcp::Connection::Factory &factory, Shared &shared)
    : handler_{handler}, session_id_{session_id}, server_{roq::web::rest::ServerFactory::create(*this, factory)},
      shared_{shared} {
}

void Session::operator()(roq::Event<roq::Start> const &) {
}

void Session::operator()(roq::Event<roq::Stop> const &) {
}

void Session::operator()(roq::Event<roq::Timer> const &) {
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::BusinessMessageReject> const &) {
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReject> const &) {
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::ExecutionReport> const &) {
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
      if (path[0] == "symbols"sv)
        get_symbols(response, request);
      break;
    case HEAD:
      break;
    case POST:
      break;
    case PUT:
      break;
    case DELETE:
      break;
    case CONNECT:
      break;
    case OPTIONS:
      break;
    case TRACE:
      break;
  }
}

void Session::get_symbols(Response &response, roq::web::rest::Server::Request const &) {
  if (std::empty(shared_.symbols)) {
    response(roq::web::http::Status::NOT_FOUND, roq::web::http::ContentType::APPLICATION_JSON, "[]"sv);
  } else {
    response(
        roq::web::http::Status::OK,
        roq::web::http::ContentType::APPLICATION_JSON,
        R"(["{}"])"sv,
        fmt::join(shared_.symbols, R"(",")"sv));
  }
}

// ws

// note!
//   using https://www.jsonrpc.org/specification
void Session::process(std::string_view const &message) {
  auto success = false;
  try {
    auto json = nlohmann::json::parse(message);  // note! not fast... you should consider some other json parser here
    auto version = json.at("jsonrpc"sv).template get<std::string_view>();
    if (version != JSONRPC_VERSION)
      throw roq::RuntimeError{R"(Invalid JSONRPC version ("{}"))"sv, version};
    auto method = json.at("method"sv).template get<std::string_view>();
    auto params = json.at("params"sv);
    auto id = json.at("id"sv);
    if (method == "new_order_single"sv) {
      new_order_single(params, id);
    } else if (method == "order_cancel_request"sv) {
      order_cancel_request(params, id);
    } else {
      send_error("unknown method", id);
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

void Session::new_order_single(auto const &params, auto const &id) {
  auto symbol = params.at("symbol"sv).template get<std::string_view>();
  send_result(symbol, id);
}

void Session::order_cancel_request(auto const &params, auto const &id) {
  auto symbol = params.at("symbol"sv).template get<std::string_view>();
  send_result(symbol, id);
}

void Session::send_result(std::string_view const &message, auto const &id) {
  auto type = id.type();
  switch (type) {
    using enum nlohmann::json::value_t;
    case string:
      send_text(
          R"({{)"
          R"("jsonrpc":"{}",)"
          R"("result":"{}",)"
          R"("id":"{}")"
          R"(}})"sv,
          JSONRPC_VERSION,
          message,
          id);
      break;
    case number_integer:
    case number_unsigned:
      send_text(
          R"({{)"
          R"("jsonrpc":"{}",)"
          R"("result":"{}",)"
          R"("id":{})"
          R"(}})"sv,
          JSONRPC_VERSION,
          message,
          id);
      break;
    default:
      roq::log::warn("Unexpected: type={}"sv, magic_enum::enum_name(type));
  }
}

void Session::send_error(std::string_view const &message, auto const &id) {
  auto type = id.type();
  switch (type) {
    using enum nlohmann::json::value_t;
    case string:
      send_text(
          R"({{)"
          R"("jsonrpc":"{}",)"
          R"("error":"{{")"
          R"("message":"{}")"
          R"("}},)"
          R"("id":"{}")"
          R"(}})"sv,
          JSONRPC_VERSION,
          message,
          id);
      break;
    case number_integer:
    case number_unsigned:
      send_text(
          R"({{)"
          R"("jsonrpc":"{}",)"
          R"("error":"{{")"
          R"("message":"{}")"
          R"("}},)"
          R"("id":{})"
          R"(}})"sv,
          JSONRPC_VERSION,
          message,
          id);
      break;
    default:
      roq::log::warn("Unexpected: type={}"sv, magic_enum::enum_name(type));
  }
}

template <typename... Args>
void Session::send_text(fmt::format_string<Args...> const &fmt, Args &&...args) {
  shared_.encode_buffer.clear();
  fmt::format_to(std::back_inserter(shared_.encode_buffer), fmt, std::forward<Args>(args)...);
  (*server_).send_text(shared_.encode_buffer);
}

}  // namespace json
}  // namespace simple
