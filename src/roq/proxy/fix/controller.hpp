/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

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
  // io::sys::Signal::Handler
  void operator()(io::sys::Signal::Event const &) override;

  // io::sys::Timer::Handler
  void operator()(io::sys::Timer::Event const &) override;

  // server::Session::Handler
  void operator()(Trace<roq::fix::codec::SecurityDefinition> const &) override;
  void operator()(Trace<roq::fix::codec::BusinessMessageReject> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::MarketDataRequestReject> const &, std::string_view const &username) override;
  void operator()(
      Trace<roq::fix::codec::MarketDataSnapshotFullRefresh> const &, std::string_view const &username) override;
  void operator()(
      Trace<roq::fix::codec::MarketDataIncrementalRefresh> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::OrderCancelReject> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::ExecutionReport> const &, std::string_view const &username) override;

  // client::Session::Handler
  void operator()(Trace<roq::fix::codec::OrderStatusRequest> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::MarketDataRequest> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::NewOrderSingle> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::OrderCancelReplaceRequest> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::OrderCancelRequest> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::OrderMassStatusRequest> const &, std::string_view const &username) override;
  void operator()(Trace<roq::fix::codec::OrderMassCancelRequest> const &, std::string_view const &username) override;

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

}  // namespace fix
}  // namespace proxy
}  // namespace roq
