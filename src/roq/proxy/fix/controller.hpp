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

#include "roq/proxy/fix/server/manager.hpp"
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
  void operator()(Trace<server::Session::Ready> const &, std::string_view const &username) override;
  void operator()(Trace<server::Session::Disconnected> const &, std::string_view const &username) override;
  //
  void operator()(Trace<codec::fix::BusinessMessageReject> const &, std::string_view const &username) override;
  // - user
  void operator()(Trace<codec::fix::UserResponse> const &, std::string_view const &username) override;
  // - market data
  void operator()(Trace<codec::fix::SecurityList> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::SecurityDefinition> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::SecurityStatus> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::MarketDataRequestReject> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::MarketDataSnapshotFullRefresh> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::MarketDataIncrementalRefresh> const &, std::string_view const &username) override;
  // - order management
  void operator()(Trace<codec::fix::OrderCancelReject> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::ExecutionReport> const &, std::string_view const &username) override;
  // - position management
  void operator()(Trace<codec::fix::RequestForPositionsAck> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::PositionReport> const &, std::string_view const &username) override;

  // client::Session::Handler
  void operator()(Trace<client::Session::Disconnected> const &, std::string_view const &username) override;
  // - user management
  void operator()(
      Trace<codec::fix::UserRequest> const &, std::string_view const &username, uint64_t session_id) override;
  // - market data
  void operator()(Trace<codec::fix::SecurityListRequest> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::SecurityDefinitionRequest> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::SecurityStatusRequest> const &, std::string_view const &username) override;
  void operator()(
      Trace<codec::fix::MarketDataRequest> const &, std::string_view const &username, uint64_t session_id) override;
  // - order management
  void operator()(Trace<codec::fix::OrderStatusRequest> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::NewOrderSingle> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::OrderCancelReplaceRequest> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::OrderCancelRequest> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::OrderMassStatusRequest> const &, std::string_view const &username) override;
  void operator()(Trace<codec::fix::OrderMassCancelRequest> const &, std::string_view const &username) override;
  // - position management
  void operator()(Trace<codec::fix::RequestForPositions> const &, std::string_view const &username) override;

  // utilities

  template <typename... Args>
  void dispatch(Args &&...);

  template <typename T>
  void dispatch_to_server(Trace<T> const &, std::string_view const &username);

  template <typename T>
  void dispatch_to_client(Trace<T> const &, std::string_view const &username);

  template <typename Callback>
  bool find_server_subscription(std::string_view const &md_req_id, Callback callback);

  void user_add(std::string_view const &username, uint64_t session_id);
  void user_remove(std::string_view const &username, bool ready);
  bool user_is_locked(std::string_view const &username) const;

 private:
  io::Context &context_;
  std::unique_ptr<io::sys::Signal> const terminate_;
  std::unique_ptr<io::sys::Signal> const interrupt_;
  std::unique_ptr<io::sys::Timer> const timer_;
  Shared shared_;
  server::Manager server_manager_;
  client::Manager client_manager_;
  bool ready_ = {};
  // server subscription mappings
  struct {
    // XXX TODO review if we have leaks when never receive a response
    struct {
      // state:
      absl::flat_hash_map<std::string, uint64_t> username_to_session;
      absl::flat_hash_map<uint64_t, std::string> session_to_username;
      // requests:
      // user_request_id => session id
      absl::flat_hash_map<std::string, uint64_t> server_to_client;
      // session id => user_request_id
      absl::flat_hash_map<uint64_t, std::string> client_to_server;
    } user;
    struct {
      // server subscription id => {session id, client subscription id}
      absl::flat_hash_map<std::string, std::pair<uint64_t, std::string>> server_to_client;
      // session id => client subscription id => server subscription id
      absl::flat_hash_map<uint64_t, absl::flat_hash_map<std::string, std::string>> client_to_server;
    } market_data;
  } subscriptions_;
};

}  // namespace fix
}  // namespace proxy
}  // namespace roq
