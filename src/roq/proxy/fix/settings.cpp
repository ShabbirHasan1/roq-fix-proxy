/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/proxy/fix/settings.hpp"

#include "roq/proxy/fix/flags/flags.hpp"
#include "roq/proxy/fix/flags/test.hpp"

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
  auto test = flags::Test::create();
  return {
      .config_file = flags.config_file,
      .net{
          .connection_timeout = CONNECTION_TIMEOUT,
          .tls_validate_certificate = TLS_VALIDATE_CERTIFICATE,
      },
      .auth = flags::Auth::create(),
      .server = flags::Server::create(),
      .client = flags::Client::create(),
      .test{
          .enable_order_mass_cancel = flags.enable_order_mass_cancel,
          .disable_remove_cl_ord_id = flags.disable_remove_cl_ord_id,
          .hmac_sha256 = test.hmac_sha256,
      },
  };
}

}  // namespace fix
}  // namespace proxy
}  // namespace roq
