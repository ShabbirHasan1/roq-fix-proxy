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
              .target_comp_id = flags::Flags::fix_target_comp_id(),
              .sender_comp_id = flags::Flags::fix_sender_comp_id(),
              .username = flags::Flags::fix_username(),
              .decode_buffer_size = flags::Flags::fix_decode_buffer_size(),
              .encode_buffer_size = flags::Flags::fix_encode_buffer_size(),
              .ping_freq = flags::Flags::fix_ping_freq(),
              .debug = flags::Flags::fix_debug(),
          },
  };
}

}  // namespace simple
