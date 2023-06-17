/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <memory>
#include <string>

#include "roq/api.hpp"

#include "roq/io/net/tcp/connection.hpp"

#include "roq/web/rest/server.hpp"

#include "roq/fix_bridge/fix/business_message_reject.hpp"
#include "roq/fix_bridge/fix/execution_report.hpp"
#include "roq/fix_bridge/fix/new_order_single.hpp"
#include "roq/fix_bridge/fix/order_cancel_reject.hpp"
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

  void operator()(roq::Event<roq::Start> const &);
  void operator()(roq::Event<roq::Stop> const &);
  void operator()(roq::Event<roq::Timer> const &);

  void operator()(roq::Trace<roq::fix_bridge::fix::BusinessMessageReject> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReject> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::ExecutionReport> const &);

 protected:
  // web::rest::Server::Handler
  void operator()(roq::web::rest::Server::Disconnected const &) override;
  void operator()(roq::web::rest::Server::Request const &) override;
  void operator()(roq::web::rest::Server::Text const &) override;
  void operator()(roq::web::rest::Server::Binary const &) override;

  // rest

  void route(Response &, roq::web::rest::Server::Request const &, std::span<std::string_view> const &path);

  void get_symbols(Response &, roq::web::rest::Server::Request const &);

  // ws

  void process(std::string_view const &message);

  void process_jsonrpc(std::string_view const &method, auto const &params, auto const &id);

  void new_order_single(auto const &params, auto const &id);
  void order_cancel_request(auto const &params, auto const &id);

  void send_result(std::string_view const &message, auto const &id);
  void send_error(std::string_view const &message, auto const &id);

  template <typename... Args>
  void send_text(fmt::format_string<Args...> const &, Args &&...);

 private:
  Handler &handler_;
  uint64_t const session_id_;
  std::unique_ptr<roq::web::rest::Server> server_;
  Shared &shared_;
};

}  // namespace json
}  // namespace simple
