/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/json/response.hpp"

using namespace std::literals;

namespace simple {
namespace json {

// === CONSTANTS ===

namespace {
auto const CACHE_CONTROL_NO_STORE = "no-store"sv;
}  // namespace

// === IMPLEMENTATION ===

Response::Response(roq::web::rest::Server &server, roq::web::rest::Server::Request const &request)
    : server_{server}, request_{request} {
}

void Response::operator()(roq::web::http::Status status) {
  auto connection = [&]() {
    if (status != roq::web::http::Status::OK)  // XXX maybe only close based on category ???
      return roq::web::http::Connection::CLOSE;
    return request_.headers.connection;
  }();
  auto response = roq::web::rest::Server::Response{
      .status = status,
      .connection = connection,
      .sec_websocket_accept = {},
      .cache_control = {},
      .content_type = {},
      .body = {},
  };
  server_.send(response);
};

void Response::operator()(
    roq::web::http::Status, roq::web::http::ContentType content_type, std::string_view const &body) {
  auto response = roq::web::rest::Server::Response{
      .status = roq::web::http::Status::OK,
      .connection = request_.headers.connection,
      .sec_websocket_accept = {},
      .cache_control = CACHE_CONTROL_NO_STORE,
      .content_type = content_type,
      .body = body,
  };
  server_.send(response);
}

}  // namespace json
}  // namespace simple
