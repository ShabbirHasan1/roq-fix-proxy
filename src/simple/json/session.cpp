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

Session::Session(uint64_t session_id, roq::io::net::tcp::Connection::Factory &factory, Shared &shared)
    : session_id_{session_id}, server_{roq::web::rest::ServerFactory::create(*this, factory)}, shared_{shared} {
}

// web::rest::Server::Handler

void Session::operator()(roq::web::rest::Server::Disconnected const &) {
  shared_.sessions_to_remove.emplace(session_id_);
}

void Session::operator()(roq::web::rest::Server::Request const &request) {
  try {
    if (request.headers.connection == roq::web::http::Connection::UPGRADE) {
      roq::log::info("Upgrading session_id={} to websocket..."sv, session_id_);
      (*server_).upgrade(request);
    } else {
      roq::log::info("request={}"sv, request);
      auto path = request.path;  // note! std::span<std::string_view>
      if (!std::empty(path) && !std::empty(shared_.settings.json.url_prefix) &&
          path[0] == shared_.settings.json.url_prefix)
        path = path.subspan(1);  // drop prefix
      if (!std::empty(path)) {
        switch (request.method) {
          using enum roq::web::http::Method;
          case GET:
            if (request.path[0] == "exchanges"sv)
              get_exchanges(request);
            else if (request.path[0] == "symbols"sv)
              get_symbols(request);
            break;
          case HEAD:
            break;
          case POST:
            if (request.path[0] == "order"sv)
              post_order(request);
            break;
          case PUT:
            break;
          case DELETE:
            if (request.path[0] == "order"sv)
              delete_order(request);
            break;
          case CONNECT:
            break;
          case OPTIONS:
            break;
          case TRACE:
            break;
        }
      }
      /*
      // XXX expect POST
      auto result = process(request);
      auto response = roq::web::rest::Server::Response{
          .status = roq::web::http::Status::OK,  // XXX should depend on result type
          .connection = request.headers.connection,
          .sec_websocket_accept = {},
          .cache_control = {},
          .content_type = roq::web::http::ContentType::APPLICATION_JSON,
          .body = result,
      };
      (*server_).send(response);
      */
    }
  } catch (roq::RuntimeError &e) {
    roq::log::error("Error: {}"sv, e);
    (*server_).close();
  } catch (std::exception &e) {
    roq::log::error("Error: {}"sv, e.what());
    (*server_).close();
  }
}

void Session::operator()(roq::web::rest::Server::Text const &text) {
  roq::log::info(R"(message="{})"sv, text.payload);
  try {
    // auto result = process_request(text.payload);
    // (*server_).send_text(result);
  } catch (roq::RuntimeError &e) {
    roq::log::error("Error: {}"sv, e);
    (*server_).close();
  } catch (std::exception &e) {
    roq::log::error("Error: {}"sv, e.what());
    (*server_).close();
  }
}

void Session::operator()(roq::web::rest::Server::Binary const &) {
  roq::log::warn("Unexpected"sv);
  (*server_).close();
}

// utilities

void Session::get_exchanges(roq::web::rest::Server::Request const &) {
}

void Session::get_symbols(roq::web::rest::Server::Request const &) {
}

void Session::post_order(roq::web::rest::Server::Request const &) {
}

void Session::delete_order(roq::web::rest::Server::Request const &) {
}

/*
std::string_view Session::process(roq::web::rest::Server::Request const &request) {
  auto message = request.body;
  auto json = nlohmann::json::parse(message);
  auto action = json.value("action"s, ""s);
  auto symbol = json.value("symbol"s, ""s);
  if (std::empty(action)) {
    return error("missing 'action'"sv);
  } else if (action == "subscribe"sv) {
    if (validate_symbol(symbol)) {
      // XXX maybe check if symbol already exists?
      shared_.symbols.emplace(symbol);
      return success();
    } else {
      return error("invalid symbol"sv);
    }
  } else if (action == "unsubscribe"sv) {
    if (validate_symbol(symbol)) {
      // XXX maybe check if symbol exists?
      shared_.symbols.erase(symbol);
      return success();
    } else {
      return error("invalid symbol"sv);
    }
  } else {
    return error("unknown 'action'"sv);
  }
}
*/

bool Session::validate_symbol(std::string_view const &symbol) {
  if (std::empty(symbol)) {
    return false;
  } else if (std::size(symbol) > sizeof(decltype(Shared::symbols)::value_type)) {
    return false;
  } else {
    return true;
  }
}

std::string_view Session::success() {
  return format(R"({{)"
                R"("status":"successs")"
                R"(}})"sv);
}

std::string_view Session::error(std::string_view const &text) {
  return format(
      R"({{)"
      R"("status":"error",)"
      R"("text":"{}")"
      R"(}})"sv,
      text);
}

template <typename... Args>
std::string_view Session::format(fmt::format_string<Args...> const &fmt, Args &&...args) {
  buffer_.clear();
  fmt::format_to(std::back_inserter(buffer_), fmt, std::forward<Args>(args)...);
  return std::string_view{std::data(buffer_), std::size(buffer_)};
}

}  // namespace json
}  // namespace simple
