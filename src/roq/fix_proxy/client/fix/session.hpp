/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>
#include <string>

#include "roq/io/net/tcp/connection.hpp"

#include "roq/fix_proxy/shared.hpp"

#include "roq/fix_proxy/client/session.hpp"

namespace roq {
namespace fix_proxy {
namespace client {
namespace fix {

// note! supports both rest and websocket

struct Session final : public client::Session, public io::net::tcp::Connection::Handler {
  Session(client::Session::Handler &, uint64_t session_id, roq::io::net::tcp::Connection::Factory &, Shared &);

  void operator()(roq::Trace<roq::fix_bridge::fix::BusinessMessageReject> const &) override;
  void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReject> const &) override;
  void operator()(roq::Trace<roq::fix_bridge::fix::ExecutionReport> const &) override;

 protected:
  bool ready() const;
  bool zombie() const;

  void close();

  // io::net::tcp::Connection::Handler

  void operator()(io::net::tcp::Connection::Read const &) override;
  void operator()(io::net::tcp::Connection::Disconnected const &) override;

 private:
  client::Session::Handler &handler_;
  uint64_t const session_id_;
  std::unique_ptr<io::net::tcp::Connection> connection_;
  Shared &shared_;
  enum class State { WAITING_LOGON, READY, ZOMBIE } state_ = {};
  std::string username_;
};

}  // namespace fix
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
