/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <chrono>
#include <memory>

#include "roq/start.hpp"
#include "roq/stop.hpp"
#include "roq/timer.hpp"

#include "roq/io/context.hpp"

#include "roq/proxy/fix/settings.hpp"
#include "roq/proxy/fix/shared.hpp"

#include "roq/proxy/fix/client/session.hpp"

#include "roq/proxy/fix/client/listener.hpp"

namespace roq {
namespace proxy {
namespace fix {
namespace client {

struct Manager final : public Listener::Handler {
  Manager(Session::Handler &, Settings const &, io::Context &, Shared &);

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void dispatch(auto &value) {
    for (auto &[_, item] : sessions_)
      (*item)(value);
  }

  template <typename Callback>
  void get_all_sessions(Callback callback) {
    for (auto &[_, session] : sessions_)
      callback(*session);
  }

  template <typename Callback>
  bool find(uint64_t session_id, Callback callback) {
    auto iter = sessions_.find(session_id);
    if (iter == std::end(sessions_))
      return false;
    callback(*(*iter).second);
    return true;
  }

 protected:
  // fix::Listener::Handler
  void operator()(Factory &) override;

  // utilities

  void remove_zombies(std::chrono::nanoseconds now);

 private:
  Session::Handler &handler_;
  Listener fix_listener_;
  Shared &shared_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Session>> sessions_;
  std::chrono::nanoseconds next_garbage_collection_ = {};
};

}  // namespace client
}  // namespace fix
}  // namespace proxy
}  // namespace roq
