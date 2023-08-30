/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "roq/io/context.hpp"

#include "roq/io/sys/signal.hpp"
#include "roq/io/sys/timer.hpp"

#include "roq/fix_proxy/config.hpp"
#include "roq/fix_proxy/settings.hpp"
#include "roq/fix_proxy/shared.hpp"

#include "roq/fix_proxy/server/manager.hpp"
#include "roq/fix_proxy/server/session.hpp"

#include "roq/fix_proxy/client/manager.hpp"
#include "roq/fix_proxy/client/session.hpp"

namespace roq {
namespace fix_proxy {

struct Controller final : public io::sys::Signal::Handler,
                          public io::sys::Timer::Handler,
                          public server::Session::Handler,
                          public client::Session::Handler {
  Controller(Settings const &, Config const &, io::Context &, std::span<std::string_view const> const &connections);

  void run();

 protected:
  // io::sys::Signal::Handler
  void operator()(io::sys::Signal::Event const &) override;

  // io::sys::Timer::Handler
  void operator()(io::sys::Timer::Event const &) override;

  // server::Session::Handler
  void operator()(Trace<fix_bridge::fix::SecurityDefinition> const &) override;
  void operator()(Trace<fix_bridge::fix::BusinessMessageReject> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::MarketDataRequestReject> const &, std::string_view const &username) override;
  void operator()(
      Trace<fix_bridge::fix::MarketDataSnapshotFullRefresh> const &, std::string_view const &username) override;
  void operator()(
      Trace<fix_bridge::fix::MarketDataIncrementalRefresh> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::OrderCancelReject> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::ExecutionReport> const &, std::string_view const &username) override;

  // client::Session::Handler
  void operator()(Trace<fix_bridge::fix::OrderStatusRequest> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::MarketDataRequest> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::NewOrderSingle> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::OrderCancelReplaceRequest> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::OrderCancelRequest> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::OrderMassStatusRequest> const &, std::string_view const &username) override;
  void operator()(Trace<fix_bridge::fix::OrderMassCancelRequest> const &, std::string_view const &username) override;

  // utilities

  template <typename... Args>
  void dispatch(Args &&...);

  template <typename T>
  void dispatch_to_server(Trace<T> const &, std::string_view const &username);

  template <typename T>
  void dispatch_to_client(Trace<T> const &, std::string_view const &username);

 private:
  io::Context &context_;
  std::unique_ptr<io::sys::Signal> const terminate_;
  std::unique_ptr<io::sys::Signal> const interrupt_;
  std::unique_ptr<io::sys::Timer> const timer_;
  Shared shared_;
  server::Manager server_manager_;
  client::Manager client_manager_;
};

}  // namespace fix_proxy
}  // namespace roq
