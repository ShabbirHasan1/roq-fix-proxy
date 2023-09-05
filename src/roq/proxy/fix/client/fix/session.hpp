/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "roq/io/buffer.hpp"

#include "roq/io/net/tcp/connection.hpp"

// session
#include "roq/fix/codec/heartbeat.hpp"
#include "roq/fix/codec/logon.hpp"
#include "roq/fix/codec/logout.hpp"
#include "roq/fix/codec/reject.hpp"
#include "roq/fix/codec/resend_request.hpp"
#include "roq/fix/codec/test_request.hpp"

// business
#include "roq/fix/codec/market_data_request.hpp"
#include "roq/fix/codec/new_order_single.hpp"
#include "roq/fix/codec/order_cancel_replace_request.hpp"
#include "roq/fix/codec/order_cancel_request.hpp"
#include "roq/fix/codec/order_mass_cancel_request.hpp"
#include "roq/fix/codec/order_mass_status_request.hpp"
#include "roq/fix/codec/order_status_request.hpp"
#include "roq/fix/codec/request_for_positions.hpp"
#include "roq/fix/codec/security_definition_request.hpp"
#include "roq/fix/codec/security_list_request.hpp"
#include "roq/fix/codec/security_status_request.hpp"
#include "roq/fix/codec/trade_capture_report_request.hpp"
#include "roq/fix/codec/trading_session_status_request.hpp"

#include "roq/proxy/fix/shared.hpp"

#include "roq/proxy/fix/client/session.hpp"

namespace roq {
namespace proxy {
namespace fix {
namespace client {
namespace fix {

// note! supports both rest and websocket

struct Session final : public client::Session, public io::net::tcp::Connection::Handler {
  Session(client::Session::Handler &, uint64_t session_id, io::net::tcp::Connection::Factory &, Shared &);

  void operator()(Event<Stop> const &) override;
  void operator()(Event<Timer> const &) override;

  void operator()(Trace<roq::fix::codec::BusinessMessageReject> const &) override;
  void operator()(Trace<roq::fix::codec::MarketDataRequestReject> const &) override;
  void operator()(Trace<roq::fix::codec::MarketDataSnapshotFullRefresh> const &) override;
  void operator()(Trace<roq::fix::codec::MarketDataIncrementalRefresh> const &) override;
  void operator()(Trace<roq::fix::codec::OrderCancelReject> const &) override;
  void operator()(Trace<roq::fix::codec::ExecutionReport> const &) override;

 protected:
  bool ready() const;
  bool zombie() const;

  void close();

  // io::net::tcp::Connection::Handler

  void operator()(io::net::tcp::Connection::Read const &) override;
  void operator()(io::net::tcp::Connection::Disconnected const &) override;

  // utilities

  void make_zombie();

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

  void operator()(Trace<roq::fix::codec::Logon> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::Logout> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::TestRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::ResendRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::Reject> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::Heartbeat> const &, roq::fix::Header const &);

  void operator()(Trace<roq::fix::codec::TradingSessionStatusRequest> const &, roq::fix::Header const &);

  void operator()(Trace<roq::fix::codec::SecurityListRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::SecurityDefinitionRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::SecurityStatusRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::MarketDataRequest> const &, roq::fix::Header const &);

  void operator()(Trace<roq::fix::codec::OrderStatusRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::OrderMassStatusRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::NewOrderSingle> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::OrderCancelRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::OrderCancelReplaceRequest> const &, roq::fix::Header const &);
  void operator()(Trace<roq::fix::codec::OrderMassCancelRequest> const &, roq::fix::Header const &);

  void operator()(Trace<roq::fix::codec::TradeCaptureReportRequest> const &, roq::fix::Header const &);

  void operator()(Trace<roq::fix::codec::RequestForPositions> const &, roq::fix::Header const &);

  void send_reject(roq::fix::Header const &, roq::fix::SessionRejectReason, std::string_view const &text);

  void send_business_message_reject(
      roq::fix::Header const &, roq::fix::BusinessRejectReason, std::string_view const &text);

  template <typename T, typename Callback>
  bool add_party_ids(Trace<T> const &, Callback) const;

 private:
  client::Session::Handler &handler_;
  uint64_t const session_id_;
  std::unique_ptr<io::net::tcp::Connection> connection_;
  Shared &shared_;
  io::Buffer buffer_;
  std::chrono::nanoseconds const logon_timeout_;
  enum class State { WAITING_LOGON, READY, ZOMBIE } state_ = {};
  struct {
    uint64_t msg_seq_num = {};
  } outbound_;
  struct {
    uint64_t msg_seq_num = {};
  } inbound_;
  std::string comp_id_;
  std::string username_;
  std::string strategy_id_;
  roq::fix::codec::Party party_;
  std::chrono::nanoseconds next_heartbeat_ = {};
  bool waiting_for_heartbeat_ = {};
  // buffer
  std::vector<std::byte> decode_buffer_;
  std::vector<std::byte> encode_buffer_;
};

}  // namespace fix
}  // namespace client
}  // namespace fix
}  // namespace proxy
}  // namespace roq
