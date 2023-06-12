/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/web/rest/server.hpp"

namespace simple {
namespace json {

// helper

struct Response final {
  Response(roq::web::rest::Server &, roq::web::rest::Server::Request const &);

 protected:
  void operator()(roq::web::http::Status);
  void operator()(roq::web::http::Status, roq::web::http::ContentType, std::string_view const &body);

 private:
  roq::web::rest::Server &server_;
  roq::web::rest::Server::Request const &request_;
};

}  // namespace json
}  // namespace simple
