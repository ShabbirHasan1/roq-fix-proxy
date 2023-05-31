/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <chrono>
#include <memory>
#include <vector>

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
                          public roq::io::sys::Timer::Handler {
  Controller(Settings const &, Config const &, roq::io::Context &, std::span<std::string_view> const &connections);

  Controller(Controller &&) = default;
  Controller(Controller const &) = delete;

  void run();

 protected:
  // io::sys::Signal::Handler
  void operator()(roq::io::sys::Signal::Event const &) override;

  // io::sys::Timer::Handler
  void operator()(roq::io::sys::Timer::Event const &) override;

  // io::net::tcp::Listener::Handler
  void operator()(roq::io::net::tcp::Connection::Factory &) override;

  // utilities

  void remove_zombies(std::chrono::nanoseconds now);

  template <typename... Args>
  void dispatch(Args &&...);

 private:
  roq::io::Context &context_;
  std::unique_ptr<roq::io::sys::Signal> terminate_;
  std::unique_ptr<roq::io::sys::Signal> interrupt_;
  std::unique_ptr<roq::io::sys::Timer> timer_;
  // fix
  std::vector<std::unique_ptr<fix::Session>> fix_sessions_;
  // json
  std::unique_ptr<roq::io::net::tcp::Listener> const json_listener_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<json::Session>> json_sessions_;
  std::chrono::nanoseconds next_garbage_collection_ = {};
  uint64_t next_session_id_ = {};
  Shared shared_;
};

}  // namespace simple
