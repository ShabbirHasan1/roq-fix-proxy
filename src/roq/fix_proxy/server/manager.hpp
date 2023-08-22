/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>

#include <memory>
#include <span>
#include <string_view>

#include "roq/io/context.hpp"

#include "roq/fix_proxy/settings.hpp"
#include "roq/fix_proxy/shared.hpp"

#include "roq/fix_proxy/server/session.hpp"

namespace roq {
namespace fix_proxy {
namespace server {

struct Manager final {
  Manager(
      Session::Handler &,
      Settings const &,
      io::Context &,
      Shared &,
      std::span<std::string_view const> const &connections);

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

  void dispatch(auto &value) {
    for (auto &[_, item] : sessions_)
      (*item)(value);
  }

  template <typename Callback>
  bool find(std::string_view const &username, Callback callback) {
    auto iter = sessions_.find(username);
    if (iter == std::end(sessions_))
      return false;
    callback(*(*iter).second);
    return true;
  }

 private:
  absl::flat_hash_map<std::string, std::unique_ptr<Session>> sessions_;
};

}  // namespace server
}  // namespace fix_proxy
}  // namespace roq
