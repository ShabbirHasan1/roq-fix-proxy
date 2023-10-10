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

#include "roq/codec/fix/business_message_reject.hpp"
#include "roq/codec/fix/execution_report.hpp"
#include "roq/codec/fix/heartbeat.hpp"
#include "roq/codec/fix/logon.hpp"
#include "roq/codec/fix/logout.hpp"
#include "roq/codec/fix/market_data_incremental_refresh.hpp"
#include "roq/codec/fix/market_data_request.hpp"
#include "roq/codec/fix/market_data_request_reject.hpp"
#include "roq/codec/fix/market_data_snapshot_full_refresh.hpp"
#include "roq/codec/fix/new_order_single.hpp"
#include "roq/codec/fix/order_cancel_reject.hpp"
#include "roq/codec/fix/order_cancel_replace_request.hpp"
#include "roq/codec/fix/order_cancel_request.hpp"
#include "roq/codec/fix/order_mass_cancel_report.hpp"
#include "roq/codec/fix/order_mass_cancel_request.hpp"
#include "roq/codec/fix/order_mass_status_request.hpp"
#include "roq/codec/fix/order_status_request.hpp"
#include "roq/codec/fix/position_report.hpp"
#include "roq/codec/fix/reject.hpp"
#include "roq/codec/fix/request_for_positions.hpp"
#include "roq/codec/fix/request_for_positions_ack.hpp"
#include "roq/codec/fix/resend_request.hpp"
#include "roq/codec/fix/security_definition.hpp"
#include "roq/codec/fix/security_definition_request.hpp"
#include "roq/codec/fix/security_list.hpp"
#include "roq/codec/fix/security_list_request.hpp"
#include "roq/codec/fix/security_status.hpp"
#include "roq/codec/fix/security_status_request.hpp"
#include "roq/codec/fix/test_request.hpp"
#include "roq/codec/fix/user_request.hpp"
#include "roq/codec/fix/user_response.hpp"

#include "roq/proxy/fix/settings.hpp"
#include "roq/proxy/fix/shared.hpp"

namespace roq {
namespace proxy {
namespace fix {
namespace server {

// note! supports both rest and websocket

struct Session final : public io::net::ConnectionManager::Handler {
  struct Ready final {};
  struct Disconnected final {};
  struct Handler {
    virtual void operator()(Trace<Ready> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<Disconnected> const &, std::string_view const &username) = 0;
    //
    virtual void operator()(Trace<codec::fix::BusinessMessageReject> const &, std::string_view const &username) = 0;
    // user management
    virtual void operator()(Trace<codec::fix::UserResponse> const &, std::string_view const &username) = 0;
    // market data
    virtual void operator()(Trace<codec::fix::SecurityList> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::SecurityDefinition> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::SecurityStatus> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::MarketDataRequestReject> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<codec::fix::MarketDataSnapshotFullRefresh> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<codec::fix::MarketDataIncrementalRefresh> const &, std::string_view const &username) = 0;
    // order management
    virtual void operator()(Trace<codec::fix::OrderCancelReject> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::OrderMassCancelReport> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::ExecutionReport> const &, std::string_view const &username) = 0;
    // position management
    virtual void operator()(Trace<codec::fix::RequestForPositionsAck> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::PositionReport> const &, std::string_view const &username) = 0;
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

  // user management
  void operator()(Trace<codec::fix::UserRequest> const &);
  // market data
  void operator()(Trace<codec::fix::SecurityListRequest> const &);
  void operator()(Trace<codec::fix::SecurityDefinitionRequest> const &);
  void operator()(Trace<codec::fix::SecurityStatusRequest> const &);
  void operator()(Trace<codec::fix::MarketDataRequest> const &);
  // order management
  void operator()(Trace<codec::fix::OrderStatusRequest> const &);
  void operator()(Trace<codec::fix::NewOrderSingle> const &);
  void operator()(Trace<codec::fix::OrderCancelReplaceRequest> const &);
  void operator()(Trace<codec::fix::OrderCancelRequest> const &);
  void operator()(Trace<codec::fix::OrderMassStatusRequest> const &);
  void operator()(Trace<codec::fix::OrderMassCancelRequest> const &);
  // position management
  void operator()(Trace<codec::fix::RequestForPositions> const &);

 private:
  enum class State;

 protected:
  void operator()(State);

  // io::net::ConnectionManager::Handler
  void operator()(io::net::ConnectionManager::Connected const &) override;
  void operator()(io::net::ConnectionManager::Disconnected const &) override;
  void operator()(io::net::ConnectionManager::Read const &) override;

  // inbound

  void check(roq::fix::Header const &);

  void parse(Trace<roq::fix::Message> const &);

  template <typename T>
  void dispatch(Trace<roq::fix::Message> const &, T const &);

  // - session

  void operator()(Trace<codec::fix::Reject> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::ResendRequest> const &, roq::fix::Header const &);

  void operator()(Trace<codec::fix::Logon> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::Logout> const &, roq::fix::Header const &);

  void operator()(Trace<codec::fix::Heartbeat> const &, roq::fix::Header const &);

  void operator()(Trace<codec::fix::TestRequest> const &, roq::fix::Header const &);

  // - business

  void operator()(Trace<codec::fix::BusinessMessageReject> const &, roq::fix::Header const &);

  // - market data

  void operator()(Trace<codec::fix::SecurityList> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::SecurityDefinition> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::SecurityStatus> const &, roq::fix::Header const &);

  void operator()(Trace<codec::fix::MarketDataRequestReject> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::MarketDataSnapshotFullRefresh> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::MarketDataIncrementalRefresh> const &, roq::fix::Header const &);

  // - user management

  void operator()(Trace<codec::fix::UserResponse> const &, roq::fix::Header const &);

  // - order management

  void operator()(Trace<codec::fix::OrderCancelReject> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::OrderMassCancelReport> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::ExecutionReport> const &, roq::fix::Header const &);

  // - position management

  void operator()(Trace<codec::fix::RequestForPositionsAck> const &, roq::fix::Header const &);
  void operator()(Trace<codec::fix::PositionReport> const &, roq::fix::Header const &);

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
  bool const enable_market_data_;
};

}  // namespace server
}  // namespace fix
}  // namespace proxy
}  // namespace roq
