/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string_view>

#include "roq/args/parser.hpp"

#include "roq/proxy/fix/flags/auth.hpp"
#include "roq/proxy/fix/flags/client.hpp"
#include "roq/proxy/fix/flags/server.hpp"

namespace roq {
namespace proxy {
namespace fix {

struct Settings final {
  // note! dependency on args::Parser to enforce correct sequencing
  static Settings create(args::Parser const &);

  std::string_view config_file;

  struct {
    std::chrono::nanoseconds connection_timeout = {};
    bool tls_validate_certificate = {};
  } net;

  flags::Auth auth;
  flags::Server server;
  flags::Client client;

  struct {
    bool enable_order_mass_cancel = {};
    bool disable_remove_cl_ord_id = {};
    bool hmac_sha256 = {};
  } test;
};

}  // namespace fix
}  // namespace proxy
}  // namespace roq
