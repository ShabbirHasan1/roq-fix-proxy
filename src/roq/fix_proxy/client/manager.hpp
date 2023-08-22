/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <chrono>
#include <memory>

#include "roq/start.hpp"
#include "roq/stop.hpp"
#include "roq/timer.hpp"

#include "roq/io/context.hpp"

#include "roq/fix_proxy/settings.hpp"
#include "roq/fix_proxy/shared.hpp"

#include "roq/fix_proxy/client/session.hpp"

#include "roq/fix_proxy/client/fix/listener.hpp"

#include "roq/fix_proxy/client/json/listener.hpp"

namespace roq {
namespace fix_proxy {
namespace client {

struct Manager final : public fix::Listener::Handler, public json::Listener::Handler {
  Manager(Session::Handler &, Settings const &, io::Context &, Shared &);

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void dispatch(auto &value) {
    for (auto &[_, item] : sessions_)
      (*item)(value);
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
  // json::Listener::Handler
  void operator()(Factory &) override;

  // utilities

  void remove_zombies(std::chrono::nanoseconds now);

 private:
  Session::Handler &handler_;
  fix::Listener fix_listener_;
  json::Listener json_listener_;
  Shared &shared_;
  absl::flat_hash_map<uint64_t, std::unique_ptr<Session>> sessions_;
  std::chrono::nanoseconds next_garbage_collection_ = {};
};

}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
