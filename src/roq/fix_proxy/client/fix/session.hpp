/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "roq/io/buffer.hpp"

#include "roq/io/net/tcp/connection.hpp"

// session
#include "roq/fix_bridge/fix/heartbeat.hpp"
#include "roq/fix_bridge/fix/logon.hpp"
#include "roq/fix_bridge/fix/logout.hpp"
#include "roq/fix_bridge/fix/reject.hpp"
#include "roq/fix_bridge/fix/resend_request.hpp"
#include "roq/fix_bridge/fix/test_request.hpp"

// business
#include "roq/fix_bridge/fix/market_data_request.hpp"
#include "roq/fix_bridge/fix/new_order_single.hpp"
#include "roq/fix_bridge/fix/order_cancel_replace_request.hpp"
#include "roq/fix_bridge/fix/order_cancel_request.hpp"
#include "roq/fix_bridge/fix/order_mass_cancel_request.hpp"
#include "roq/fix_bridge/fix/order_mass_status_request.hpp"
#include "roq/fix_bridge/fix/order_status_request.hpp"
#include "roq/fix_bridge/fix/request_for_positions.hpp"
#include "roq/fix_bridge/fix/security_definition_request.hpp"
#include "roq/fix_bridge/fix/security_list_request.hpp"
#include "roq/fix_bridge/fix/security_status_request.hpp"
#include "roq/fix_bridge/fix/trade_capture_report_request.hpp"
#include "roq/fix_bridge/fix/trading_session_status_request.hpp"

#include "roq/fix_proxy/shared.hpp"

#include "roq/fix_proxy/client/session.hpp"

namespace roq {
namespace fix_proxy {
namespace client {
namespace fix {

// note! supports both rest and websocket

struct Session final : public client::Session, public io::net::tcp::Connection::Handler {
  Session(client::Session::Handler &, uint64_t session_id, io::net::tcp::Connection::Factory &, Shared &);

  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(Trace<fix_bridge::fix::BusinessMessageReject> const &) override;
  void operator()(Trace<fix_bridge::fix::OrderCancelReject> const &) override;
  void operator()(Trace<fix_bridge::fix::ExecutionReport> const &) override;

 protected:
  bool ready() const;
  bool zombie() const;

  void close();

  // io::net::tcp::Connection::Handler

  void operator()(io::net::tcp::Connection::Read const &) override;
  void operator()(io::net::tcp::Connection::Disconnected const &) override;

  // utilities

  // - send
  template <std::size_t level, typename T>
  void send_and_close(T const &);
  template <std::size_t level, typename T>
  void send(T const &);
  template <std::size_t level, typename T>
  void send(T const &, std::chrono::nanoseconds sending_time);

  // - receive
  void check(roq::fix::Header const &);

  void parse(Trace<roq::fix::Message> const &);

  template <typename T, typename... Args>
  void dispatch(Trace<roq::fix::Message> const &, Args &&...);

  void operator()(Trace<fix_bridge::fix::Logon> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::Logout> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::TestRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::ResendRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::Reject> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::Heartbeat> const &, roq::fix::Header const &);

  void operator()(Trace<fix_bridge::fix::TradingSessionStatusRequest> const &, roq::fix::Header const &);

  void operator()(Trace<fix_bridge::fix::SecurityListRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::SecurityDefinitionRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::SecurityStatusRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::MarketDataRequest> const &, roq::fix::Header const &);

  void operator()(Trace<fix_bridge::fix::OrderStatusRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::OrderMassStatusRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::NewOrderSingle> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::OrderCancelRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::OrderCancelReplaceRequest> const &, roq::fix::Header const &);
  void operator()(Trace<fix_bridge::fix::OrderMassCancelRequest> const &, roq::fix::Header const &);

  void operator()(Trace<fix_bridge::fix::TradeCaptureReportRequest> const &, roq::fix::Header const &);

  void operator()(Trace<fix_bridge::fix::RequestForPositions> const &, roq::fix::Header const &);

 private:
  client::Session::Handler &handler_;
  uint64_t const session_id_;
  std::unique_ptr<io::net::tcp::Connection> connection_;
  Shared &shared_;
  io::Buffer buffer_;
  enum class State { WAITING_LOGON, READY, ZOMBIE } state_ = {};
  struct {
    uint64_t msg_seq_num = {};
  } outbound_;
  struct {
    uint64_t msg_seq_num = {};
  } inbound_;
  std::string comp_id_;
  // buffer
  std::vector<std::byte> decode_buffer_;
  std::vector<std::byte> encode_buffer_;
};

}  // namespace fix
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
