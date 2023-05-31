/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>
#include <vector>

#include "roq/connection_status.hpp"
#include "roq/event.hpp"
#include "roq/start.hpp"
#include "roq/stop.hpp"
#include "roq/timer.hpp"
#include "roq/trace.hpp"

#include "roq/io/context.hpp"

#include "roq/io/web/uri.hpp"

#include "roq/io/net/connection_factory.hpp"
#include "roq/io/net/connection_manager.hpp"

#include "roq/fix/message.hpp"

#include "roq/fix_bridge/fix/execution_report.hpp"
#include "roq/fix_bridge/fix/heartbeat.hpp"
#include "roq/fix_bridge/fix/logon.hpp"
#include "roq/fix_bridge/fix/logout.hpp"
#include "roq/fix_bridge/fix/order_cancel_reject.hpp"
#include "roq/fix_bridge/fix/reject.hpp"
#include "roq/fix_bridge/fix/resend_request.hpp"
#include "roq/fix_bridge/fix/test_request.hpp"

#include "simple/settings.hpp"

namespace simple {
namespace fix {

// note! supports both rest and websocket

struct Session final : public roq::io::net::ConnectionManager::Handler {
  Session(Settings const &, roq::io::Context &, roq::io::web::URI const &);

  void operator()(roq::Event<roq::Start> const &);
  void operator()(roq::Event<roq::Stop> const &);
  void operator()(roq::Event<roq::Timer> const &);

 protected:
  bool ready() const;

  void operator()(roq::ConnectionStatus);

  // io::net::ConnectionManager::Handler
  void operator()(roq::io::net::ConnectionManager::Connected const &) override;
  void operator()(roq::io::net::ConnectionManager::Disconnected const &) override;
  void operator()(roq::io::net::ConnectionManager::Read const &) override;

  // inbound

  void check(roq::fix::Header const &);

  void parse(roq::Trace<roq::fix::Message> const &);

  template <typename T>
  void dispatch(roq::Trace<roq::fix::Message> const &, T const &);

  void operator()(roq::Trace<roq::fix_bridge::fix::Heartbeat> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::Logon> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::Logout> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::ResendRequest> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::TestRequest> const &);

  void operator()(roq::Trace<roq::fix_bridge::fix::ExecutionReport> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReject> const &);
  void operator()(roq::Trace<roq::fix_bridge::fix::Reject> const &);

  // outbound

  template <typename T>
  void send(T const &);

  void send_logon();
  void send_logout(std::string_view const &text);
  void send_test_request(std::chrono::nanoseconds now);
  void send_heartbeat(std::string_view const &test_req_id);

 private:
  // config
  std::string_view const username_;
  std::string_view const password_;
  std::string_view const sender_comp_id_;
  std::string_view const target_comp_id_;
  std::chrono::nanoseconds const ping_freq_;
  bool const debug_;
  // connection
  std::unique_ptr<roq::io::net::ConnectionFactory> const connection_factory_;
  std::unique_ptr<roq::io::net::ConnectionManager> const connection_manager_;
  // messaging
  struct {
    uint64_t msg_seq_num = {};
  } inbound_;
  struct {
    uint64_t msg_seq_num = {};
  } outbound_;
  std::vector<std::byte> decode_buffer_;
  std::vector<std::byte> encode_buffer_;
  // state
  roq::ConnectionStatus connection_status_ = {};
  std::chrono::nanoseconds next_heartbeat_ = {};
};

}  // namespace fix
}  // namespace simple
