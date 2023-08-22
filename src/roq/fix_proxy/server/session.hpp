/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "roq/api.hpp"

#include "roq/io/context.hpp"

#include "roq/io/web/uri.hpp"

#include "roq/io/net/connection_factory.hpp"
#include "roq/io/net/connection_manager.hpp"

#include "roq/fix/message.hpp"

// inbound
#include "roq/fix_bridge/fix/heartbeat.hpp"
#include "roq/fix_bridge/fix/logon.hpp"
#include "roq/fix_bridge/fix/logout.hpp"
#include "roq/fix_bridge/fix/market_data_incremental_refresh.hpp"
#include "roq/fix_bridge/fix/market_data_request_reject.hpp"
#include "roq/fix_bridge/fix/market_data_snapshot_full_refresh.hpp"
#include "roq/fix_bridge/fix/new_order_single.hpp"
#include "roq/fix_bridge/fix/order_cancel_replace_request.hpp"
#include "roq/fix_bridge/fix/order_cancel_request.hpp"
#include "roq/fix_bridge/fix/order_mass_cancel_request.hpp"
#include "roq/fix_bridge/fix/order_mass_status_request.hpp"
#include "roq/fix_bridge/fix/order_status_request.hpp"
#include "roq/fix_bridge/fix/reject.hpp"
#include "roq/fix_bridge/fix/resend_request.hpp"
#include "roq/fix_bridge/fix/security_definition.hpp"
#include "roq/fix_bridge/fix/security_list.hpp"
#include "roq/fix_bridge/fix/test_request.hpp"

// outbound
#include "roq/fix_bridge/fix/business_message_reject.hpp"
#include "roq/fix_bridge/fix/execution_report.hpp"
#include "roq/fix_bridge/fix/order_cancel_reject.hpp"

#include "roq/fix_proxy/settings.hpp"
#include "roq/fix_proxy/shared.hpp"

namespace roq {
namespace fix_proxy {
namespace server {

// note! supports both rest and websocket

struct Session final : public io::net::ConnectionManager::Handler {
  struct Handler {
    virtual void operator()(Trace<fix_bridge::fix::SecurityDefinition> const &) = 0;
    virtual void operator()(
        Trace<fix_bridge::fix::BusinessMessageReject> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<fix_bridge::fix::OrderCancelReject> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<fix_bridge::fix::ExecutionReport> const &, std::string_view const &username) = 0;
  };

  Session(
      Handler &,
      Settings const &,
      io::Context &,
      Shared &,
      io::web::URI const &,
      std::string_view const &username,
      std::string_view const &password);

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  bool ready() const;

  void operator()(Trace<fix_bridge::fix::OrderStatusRequest> const &);
  void operator()(Trace<fix_bridge::fix::NewOrderSingle> const &);
  void operator()(Trace<fix_bridge::fix::OrderCancelReplaceRequest> const &);
  void operator()(Trace<fix_bridge::fix::OrderCancelRequest> const &);
  void operator()(Trace<fix_bridge::fix::OrderMassStatusRequest> const &);
  void operator()(Trace<fix_bridge::fix::OrderMassCancelRequest> const &);

 private:
  enum class State;

 protected:
  void operator()(State);

  // io::net::ConnectionManager::Handler
  void operator()(io::net::ConnectionManager::Connected const &) override;
  void operator()(io::net::ConnectionManager::Disconnected const &) override;
  void operator()(io::net::ConnectionManager::Read const &) override;

  // inbound

  void check(fix::Header const &);

  void parse(Trace<fix::Message> const &);

  template <typename T>
  void dispatch(Trace<fix::Message> const &, T const &);

  // - session

  void operator()(Trace<fix_bridge::fix::Reject> const &, fix::Header const &);
  void operator()(Trace<fix_bridge::fix::ResendRequest> const &, fix::Header const &);

  void operator()(Trace<fix_bridge::fix::Logon> const &, fix::Header const &);
  void operator()(Trace<fix_bridge::fix::Logout> const &, fix::Header const &);

  void operator()(Trace<fix_bridge::fix::Heartbeat> const &, fix::Header const &);

  void operator()(Trace<fix_bridge::fix::TestRequest> const &, fix::Header const &);

  // - business

  void operator()(Trace<fix_bridge::fix::BusinessMessageReject> const &, fix::Header const &);

  void operator()(Trace<fix_bridge::fix::SecurityList> const &, fix::Header const &);
  void operator()(Trace<fix_bridge::fix::SecurityDefinition> const &, fix::Header const &);

  void operator()(Trace<fix_bridge::fix::MarketDataRequestReject> const &, fix::Header const &);
  void operator()(Trace<fix_bridge::fix::MarketDataSnapshotFullRefresh> const &, fix::Header const &);
  void operator()(Trace<fix_bridge::fix::MarketDataIncrementalRefresh> const &, fix::Header const &);

  void operator()(Trace<fix_bridge::fix::OrderCancelReject> const &, fix::Header const &);

  void operator()(Trace<fix_bridge::fix::ExecutionReport> const &, fix::Header const &);

  // outbound

  template <typename T>
  void send(T const &value);

  template <typename T>
  void send_helper(T const &value);

  void send_logon();
  void send_logout(std::string_view const &text);
  void send_heartbeat(std::string_view const &test_req_id);
  void send_test_request(std::chrono::nanoseconds now);

  void send_security_list_request();
  void send_security_definition_request(std::string_view const &exchange, std::string_view const &symbol);

  void send_market_data_request(std::string_view const &exchange, std::string_view const &symbol);

  // download

  void download_security_list();

 private:
  Handler &handler_;
  Shared &shared_;
  // config
  std::string_view const username_;
  std::string_view const password_;
  std::string_view const sender_comp_id_;
  std::string_view const target_comp_id_;
  std::chrono::nanoseconds const ping_freq_;
  bool const debug_;
  uint32_t const market_depth_;
  // connection
  std::unique_ptr<io::net::ConnectionFactory> const connection_factory_;
  std::unique_ptr<io::net::ConnectionManager> const connection_manager_;
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
  enum class State {
    DISCONNECTED,
    LOGON_SENT,
    GET_SECURITY_LIST,
    READY,
  } state_ = {};
  std::chrono::nanoseconds next_heartbeat_ = {};
  absl::flat_hash_map<std::string, absl::flat_hash_set<std::string>> exchange_symbols_;
  // TEST
  bool const disable_market_data_;
};

}  // namespace server
}  // namespace fix_proxy
}  // namespace roq
