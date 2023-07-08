/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/settings.hpp"

using namespace std::chrono_literals;

namespace simple {

// === CONSTANTS ===

namespace {
auto CONNECTION_TIMEOUT = 5s;
auto TLS_VALIDATE_CERTIFICATE = false;
}  // namespace

// === IMPLEMENTATION ===

Settings Settings::create() {
  auto flags = flags::Flags::create();
  return {
      .config_file = flags.config_file,
      .net{
          .connection_timeout = CONNECTION_TIMEOUT,
          .tls_validate_certificate = TLS_VALIDATE_CERTIFICATE,
      },
      .fix = flags::FIX::create(),
      .rest = flags::REST::create(),
      .test{
          .disable_market_data = flags.disable_market_data,
      },
  };
}

}  // namespace simple
