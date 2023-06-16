/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <memory>
#include <string>

#include "roq/api.hpp"

#include "roq/io/net/tcp/connection.hpp"

#include "roq/web/rest/server.hpp"

#include "roq/fix_bridge/fix/new_order_single.hpp"
#include "roq/fix_bridge/fix/order_cancel_replace_request.hpp"
#include "roq/fix_bridge/fix/order_cancel_request.hpp"

#include "simple/shared.hpp"

#include "simple/json/response.hpp"

namespace simple {
namespace json {

// note! supports both rest and websocket

struct Session final : public roq::web::rest::Server::Handler {
  struct Handler {
    virtual void operator()(roq::Trace<roq::fix_bridge::fix::NewOrderSingle> const &) = 0;
    virtual void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReplaceRequest> const &) = 0;
    virtual void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelRequest> const &) = 0;
  };

  Session(Handler &, uint64_t session_id, roq::io::net::tcp::Connection::Factory &, Shared &);

 protected:
  // web::rest::Server::Handler
  void operator()(roq::web::rest::Server::Disconnected const &) override;
  void operator()(roq::web::rest::Server::Request const &) override;
  void operator()(roq::web::rest::Server::Text const &) override;
  void operator()(roq::web::rest::Server::Binary const &) override;

  // rest

  void route(Response &, roq::web::rest::Server::Request const &, std::span<std::string_view> const &path);

  void get_exchanges(Response &, roq::web::rest::Server::Request const &);
  void get_symbols(Response &, roq::web::rest::Server::Request const &);

  void post_order(Response &, roq::web::rest::Server::Request const &);
  void delete_order(Response &, roq::web::rest::Server::Request const &);

  // ws

  void process(std::string_view const &payload);

 private:
  Handler &handler_;
  uint64_t const session_id_;
  std::unique_ptr<roq::web::rest::Server> server_;
  Shared &shared_;
};

}  // namespace json
}  // namespace simple
