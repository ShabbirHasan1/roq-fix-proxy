/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <memory>
#include <string>

#include "roq/api.hpp"

#include "roq/io/net/tcp/connection.hpp"

#include "roq/web/rest/server.hpp"

#include "roq/fix_proxy/shared.hpp"

#include "roq/fix_proxy/client/session.hpp"

#include "roq/fix_proxy/client/json/response.hpp"

namespace roq {
namespace fix_proxy {
namespace client {
namespace json {

// note! supports both rest and websocket

struct Session final : public client::Session, public web::rest::Server::Handler {
  Session(client::Session::Handler &, uint64_t session_id, io::net::tcp::Connection::Factory &, Shared &);

  void operator()(Trace<fix_bridge::fix::BusinessMessageReject> const &) override;
  void operator()(Trace<fix_bridge::fix::OrderCancelReject> const &) override;
  void operator()(Trace<fix_bridge::fix::ExecutionReport> const &) override;

 protected:
  bool ready() const;
  bool zombie() const;

  void close();

  // web::rest::Server::Handler
  void operator()(web::rest::Server::Disconnected const &) override;
  void operator()(web::rest::Server::Request const &) override;
  void operator()(web::rest::Server::Text const &) override;
  void operator()(web::rest::Server::Binary const &) override;

  // rest

  void route(Response &, web::rest::Server::Request const &, std::span<std::string_view> const &path);

  void get_symbols(Response &, web::rest::Server::Request const &);

  // ws

  void process(std::string_view const &message);

  void process_jsonrpc(std::string_view const &method, auto const &params, auto const &id);

  // - session
  void logon(TraceInfo const &, auto const &params, auto const &id);
  void logout(TraceInfo const &, auto const &params, auto const &id);
  // - business
  // -- single order
  void order_status_request(TraceInfo const &, auto const &params, auto const &id);
  void new_order_single(TraceInfo const &, auto const &params, auto const &id);
  void order_cancel_request(TraceInfo const &, auto const &params, auto const &id);
  // -- many orders
  void order_mass_status_request(TraceInfo const &, auto const &params, auto const &id);
  void order_mass_cancel_request(TraceInfo const &, auto const &params, auto const &id);

  // helpers

  void dispatch(TraceInfo const &, auto const &value);

  void send_result(std::string_view const &message, auto const &id);
  void send_error(std::string_view const &message, auto const &id);

  void send_jsonrpc(std::string_view const &type, std::string_view const &message, auto const &id);

  template <typename... Args>
  void send_text(fmt::format_string<Args...> const &, Args &&...);

 private:
  client::Session::Handler &handler_;
  uint64_t const session_id_;
  std::unique_ptr<web::rest::Server> server_;
  Shared &shared_;
  enum class State { WAITING_LOGON, READY, ZOMBIE } state_ = {};
  std::string username_;
};

}  // namespace json
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
