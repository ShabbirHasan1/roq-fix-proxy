/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <memory>
#include <vector>

#include "roq/io/net/tcp/connection.hpp"

#include "roq/web/rest/server.hpp"

#include "simple/shared.hpp"

namespace simple {
namespace json {

// note! supports both rest and websocket

struct Session final : public roq::web::rest::Server::Handler {
  Session(uint64_t session_id, roq::io::net::tcp::Connection::Factory &, Shared &);

 protected:
  // web::rest::Server::Handler
  void operator()(roq::web::rest::Server::Disconnected const &) override;
  void operator()(roq::web::rest::Server::Request const &) override;
  void operator()(roq::web::rest::Server::Text const &) override;
  void operator()(roq::web::rest::Server::Binary const &) override;

  // utilities

  void get_exchanges(roq::web::rest::Server::Request const &);
  void get_symbols(roq::web::rest::Server::Request const &);

  void post_order(roq::web::rest::Server::Request const &);
  void delete_order(roq::web::rest::Server::Request const &);

  // std::string_view process(roq::web::rest::Server::Request const &);

  bool validate_symbol(std::string_view const &symbol);

  std::string_view success();
  std::string_view error(std::string_view const &text);

  template <typename... Args>
  std::string_view format(fmt::format_string<Args...> const &, Args &&...);

 private:
  uint64_t const session_id_;
  std::unique_ptr<roq::web::rest::Server> server_;
  Shared &shared_;
  std::vector<char> buffer_;
};

}  // namespace json
}  // namespace simple
