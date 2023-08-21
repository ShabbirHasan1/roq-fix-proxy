/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>

#include "roq/io/context.hpp"

#include "roq/fix_proxy/settings.hpp"

#include "roq/fix_proxy/client/factory.hpp"
#include "roq/fix_proxy/client/session.hpp"

namespace roq {
namespace fix_proxy {
namespace client {
namespace fix {

struct Listener final : public roq::io::net::tcp::Listener::Handler {
  struct Handler {
    virtual void operator()(Factory &) = 0;
  };

  Listener(Handler &, Settings const &, roq::io::Context &);

 protected:
  // io::net::tcp::Listener::Handler
  void operator()(roq::io::net::tcp::Connection::Factory &) override;

 private:
  Handler &handler_;
  std::unique_ptr<roq::io::net::tcp::Listener> const listener_;
};

}  // namespace fix
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
