/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <chrono>
#include <memory>
#include <vector>

#include "roq/client.hpp"

#include "roq/io/context.hpp"

#include "simple/session.hpp"
#include "simple/shared.hpp"

namespace simple {

struct Controller final : public roq::client::Handler, public roq::io::net::tcp::Listener::Handler {
  Controller(roq::client::Dispatcher &, roq::io::Context &);

  Controller(Controller &&) = default;
  Controller(Controller const &) = delete;

 protected:
  // client::Handler
  void operator()(roq::Event<roq::Timer> const &) override;

  // io::net::tcp::Listener::Handler
  void operator()(roq::io::net::tcp::Connection::Factory &) override;

  // utilities

  void remove_zombies(std::chrono::nanoseconds now);

 private:
  roq::client::Dispatcher &dispatcher_;
  roq::io::Context &context_;
  std::unique_ptr<roq::io::net::tcp::Listener> const listener_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Session>> sessions_;
  std::chrono::nanoseconds next_garbage_collection_ = {};
  uint64_t next_session_id_ = {};
  Shared shared_;
};

}  // namespace simple
