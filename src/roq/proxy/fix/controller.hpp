/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <span>
#include <string_view>

#include "roq/io/context.hpp"

#include "roq/io/sys/signal.hpp"
#include "roq/io/sys/timer.hpp"

#include "roq/proxy/fix/config.hpp"
#include "roq/proxy/fix/settings.hpp"
#include "roq/proxy/fix/shared.hpp"

#include "roq/proxy/fix/server/session.hpp"

#include "roq/proxy/fix/client/manager.hpp"
#include "roq/proxy/fix/client/session.hpp"

namespace roq {
namespace proxy {
namespace fix {

struct Controller final : public io::sys::Signal::Handler,
                          public io::sys::Timer::Handler,
                          public server::Session::Handler,
                          public client::Session::Handler {
  Controller(Settings const &, Config const &, io::Context &, std::span<std::string_view const> const &connections);

  void run();

 protected:
  bool ready() const { return ready_; }

  // io::sys::Signal::Handler
  void operator()(io::sys::Signal::Event const &) override;

  // io::sys::Timer::Handler
  void operator()(io::sys::Timer::Event const &) override;

  // server::Session::Handler
  void operator()(Trace<server::Session::Ready> const &) override;
  void operator()(Trace<server::Session::Disconnected> const &) override;
  //
  void operator()(Trace<codec::fix::BusinessMessageReject> const &) override;
  // - user
  void operator()(Trace<codec::fix::UserResponse> const &) override;
  // - security
  void operator()(Trace<codec::fix::SecurityList> const &) override;
  void operator()(Trace<codec::fix::SecurityDefinition> const &) override;
  void operator()(Trace<codec::fix::SecurityStatus> const &) override;
  // - market data
  void operator()(Trace<codec::fix::MarketDataRequestReject> const &) override;
  void operator()(Trace<codec::fix::MarketDataSnapshotFullRefresh> const &) override;
  void operator()(Trace<codec::fix::MarketDataIncrementalRefresh> const &) override;
  // - orders
  void operator()(Trace<codec::fix::OrderCancelReject> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::OrderMassCancelReport> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::ExecutionReport> const &) override;
  // - positions
  void operator()(Trace<codec::fix::RequestForPositionsAck> const &) override;
  void operator()(Trace<codec::fix::PositionReport> const &) override;
  // - trades
  void operator()(Trace<codec::fix::TradeCaptureReportRequestAck> const &) override;
  void operator()(Trace<codec::fix::TradeCaptureReport> const &) override;

  // client::Session::Handler
  void operator()(Trace<client::Session::Disconnected> const &, uint64_t session_id) override;
  // - user
  void operator()(Trace<codec::fix::UserRequest> const &, uint64_t session_id) override;
  // - security
  void operator()(Trace<codec::fix::SecurityListRequest> const &, uint64_t session_id) override;
  void operator()(Trace<codec::fix::SecurityDefinitionRequest> const &, uint64_t session_id) override;
  void operator()(Trace<codec::fix::SecurityStatusRequest> const &, uint64_t session_id) override;
  // - market data
  void operator()(Trace<codec::fix::MarketDataRequest> const &, uint64_t session_id) override;
  // - orders
  void operator()(Trace<codec::fix::OrderStatusRequest> const &, uint64_t session_id) override;
  void operator()(Trace<codec::fix::NewOrderSingle> const &, uint64_t session_id) override;
  void operator()(Trace<codec::fix::OrderCancelReplaceRequest> const &, uint64_t session_id) override;
  void operator()(Trace<codec::fix::OrderCancelRequest> const &, uint64_t session_id) override;
  void operator()(Trace<codec::fix::OrderMassStatusRequest> const &, uint64_t session_id) override;
  void operator()(Trace<codec::fix::OrderMassCancelRequest> const &, uint64_t session_id) override;
  // - positions
  void operator()(Trace<codec::fix::RequestForPositions> const &, uint64_t session_id) override;
  // - trades
  void operator()(Trace<codec::fix::TradeCaptureReportRequest> const &, uint64_t session_id) override;

  // utilities

  template <typename... Args>
  void dispatch(Args &&...);

  template <typename T>
  void dispatch_to_server(Trace<T> const &);

  template <typename T>
  void dispatch_to_client(Trace<T> const &, std::string_view const &username);

  template <typename T>
  bool dispatch_to_client(Trace<T> const &, uint64_t session_id);

  template <typename T>
  void broadcast(Trace<T> const &, std::string_view const &client_id);

  template <typename Callback>
  bool find_req_id(auto &mapping, std::string_view const &req_id, Callback callback);

  void remove_req_id(auto &mapping, std::string_view const &req_id);

  void user_add(std::string_view const &username, uint64_t session_id);
  void user_remove(std::string_view const &username, bool ready);
  bool user_is_locked(std::string_view const &username) const;

 private:
  io::Context &context_;
  std::unique_ptr<io::sys::Signal> const terminate_;
  std::unique_ptr<io::sys::Signal> const interrupt_;
  std::unique_ptr<io::sys::Timer> const timer_;
  Shared shared_;
  server::Session server_session_;
  client::Manager client_manager_;
  bool ready_ = {};
  // req_id mappings
  struct {
    struct {
      absl::flat_hash_map<std::string, uint64_t> client_to_session;
      absl::flat_hash_map<uint64_t, std::string> session_to_client;
      // user_request_id => session_id
      absl::flat_hash_map<std::string, uint64_t> server_to_client;
      // session_id => user_request_id
      absl::flat_hash_map<uint64_t, std::string> client_to_server;
    } user;
    struct Mapping final {
      // server_req_id => {session_id, client_req_id, keep_alive}
      absl::flat_hash_map<std::string, std::tuple<uint64_t, std::string, bool>> server_to_client;
      // session_id => client_req_id => server_req_id
      absl::flat_hash_map<uint64_t, absl::flat_hash_map<std::string, std::string>> client_to_server;
    };
    Mapping security_req_id;
    Mapping security_status_req_id;
    Mapping md_req_id;
    Mapping ord_status_req_id;
    Mapping mass_status_req_id;
    Mapping pos_req_id;
    Mapping trade_request_id;
  } subscriptions_;
  // WORK-AROUND
  uint32_t total_num_pos_reports_ = {};
};

}  // namespace fix
}  // namespace proxy
}  // namespace roq
