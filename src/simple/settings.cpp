/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/settings.hpp"

#include "simple/flags/flags.hpp"

using namespace std::chrono_literals;

namespace simple {

// === CONSTANTS ===

namespace {
auto CONNECTION_TIMEOUT = 5s;
auto TLS_VALIDATE_CERTIFICATE = false;
}  // namespace

// === IMPLEMENTATION ===

Settings Settings::create() {
  return {
      .config_file = flags::Flags::config_file(),
      .listen_port = flags::Flags::listen_port(),
      .net =
          {
              .connection_timeout = CONNECTION_TIMEOUT,
              .tls_validate_certificate = TLS_VALIDATE_CERTIFICATE,
          },
      .fix =
          {
              .debug = flags::Flags::fix_debug(),
          },
  };
}

}  // namespace simple
