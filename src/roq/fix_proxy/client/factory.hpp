/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>

#include "roq/fix_proxy/shared.hpp"

#include "roq/fix_proxy/client/session.hpp"

namespace roq {
namespace fix_proxy {
namespace client {

struct Factory {
  virtual std::unique_ptr<Session> create(Session::Handler &, uint64_t session_id, Shared &) = 0;
};

}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
