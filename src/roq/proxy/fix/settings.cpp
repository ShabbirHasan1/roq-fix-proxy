/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/settings.hpp"

using namespace std::chrono_literals;

namespace roq {
namespace proxy {
namespace fix {

// === CONSTANTS ===

namespace {
auto CONNECTION_TIMEOUT = 5s;
auto TLS_VALIDATE_CERTIFICATE = false;
}  // namespace

// === IMPLEMENTATION ===

Settings Settings::create(args::Parser const &) {
  auto flags = flags::Flags::create();
  return {
      .config_file = flags.config_file,
      .net{
          .connection_timeout = CONNECTION_TIMEOUT,
          .tls_validate_certificate = TLS_VALIDATE_CERTIFICATE,
      },
      .server = flags::Server::create(),
      .client = flags::Client::create(),
      .test{
          .enable_market_data = flags.enable_market_data,
          .enable_order_mass_cancel = flags.enable_order_mass_cancel,
      },
  };
}

}  // namespace fix
}  // namespace proxy
}  // namespace roq
