/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <memory>

#include "roq/api.hpp"

#include "roq/io/context.hpp"

#include "roq/io/web/uri.hpp"

#include "roq/web/socket/client.hpp"

#include "roq/proxy/fix/settings.hpp"
#include "roq/proxy/fix/shared.hpp"

namespace roq {
namespace proxy {
namespace fix {
namespace auth {

struct Session final : public web::socket::Client::Handler {
  struct Handler {};

  Session(Handler &, Settings const &, io::Context &, Shared &, io::web::URI const &);

  void operator()(Event<Start> const &);
  void operator()(Event<Stop> const &);
  void operator()(Event<Timer> const &);

 protected:
  // io::web::socket::Client::Handler
  void operator()(web::socket::Client::Connected const &) override;
  void operator()(web::socket::Client::Disconnected const &) override;
  void operator()(web::socket::Client::Ready const &) override;
  void operator()(web::socket::Client::Close const &) override;
  void operator()(web::socket::Client::Latency const &) override;
  void operator()(web::socket::Client::Text const &) override;
  void operator()(web::socket::Client::Binary const &) override;

 private:
  Handler &handler_;
  Shared &shared_;
  std::unique_ptr<web::socket::Client> const connection_;
};

}  // namespace auth
}  // namespace fix
}  // namespace proxy
}  // namespace roq
