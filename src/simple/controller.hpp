/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <chrono>
#include <memory>

#include "roq/io/context.hpp"

#include "roq/io/sys/signal.hpp"
#include "roq/io/sys/timer.hpp"

#include "simple/config.hpp"
#include "simple/settings.hpp"
#include "simple/shared.hpp"

#include "simple/fix/session.hpp"

#include "simple/json/session.hpp"

namespace simple {

struct Controller final : public roq::io::net::tcp::Listener::Handler,
                          public roq::io::sys::Signal::Handler,
                          public roq::io::sys::Timer::Handler,
                          public fix::Session::Handler,
                          public json::Session::Handler {
  Controller(Settings const &, Config const &, roq::io::Context &, std::span<std::string_view> const &connections);

  void run();

 protected:
  // io::sys::Signal::Handler
  void operator()(roq::io::sys::Signal::Event const &) override;

  // io::sys::Timer::Handler
  void operator()(roq::io::sys::Timer::Event const &) override;

  // io::net::tcp::Listener::Handler
  void operator()(roq::io::net::tcp::Connection::Factory &) override;

  // fix::Session::Handler
  void operator()(roq::Trace<roq::fix_bridge::fix::SecurityDefinition> const &) override;
  void operator()(roq::Trace<roq::fix_bridge::fix::BusinessMessageReject> const &) override;
  void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReject> const &) override;
  void operator()(roq::Trace<roq::fix_bridge::fix::ExecutionReport> const &) override;

  // json::Session::Handler
  void operator()(roq::Trace<roq::fix_bridge::fix::NewOrderSingle> const &) override;
  void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReplaceRequest> const &) override;
  void operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelRequest> const &) override;

  // utilities

  void remove_zombies(std::chrono::nanoseconds now);

  template <typename... Args>
  void dispatch(Args &&...);

 private:
  roq::io::Context &context_;
  std::unique_ptr<roq::io::sys::Signal> terminate_;
  std::unique_ptr<roq::io::sys::Signal> interrupt_;
  std::unique_ptr<roq::io::sys::Timer> timer_;
  //
  Shared shared_;
  // fix
  std::unique_ptr<fix::Session> fix_session_;
  // json
  std::unique_ptr<roq::io::net::tcp::Listener> const json_listener_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<json::Session>> json_sessions_;
  std::chrono::nanoseconds next_garbage_collection_ = {};
  uint64_t next_session_id_ = {};
};

}  // namespace simple
