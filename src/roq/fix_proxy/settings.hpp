/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string_view>

#include "roq/args/parser.hpp"

#include "roq/fix_proxy/flags/fix.hpp"
#include "roq/fix_proxy/flags/flags.hpp"
#include "roq/fix_proxy/flags/rest.hpp"

namespace roq {
namespace fix_proxy {

struct Settings final {
  // note! dependency on args::Parser to enforce correct sequencing
  static Settings create(args::Parser const &);

  std::string_view config_file;

  struct {
    std::chrono::nanoseconds connection_timeout = {};
    bool tls_validate_certificate = {};
  } net;

  flags::FIX fix;
  flags::REST rest;

  struct {
    bool disable_market_data = {};
  } test;
};

}  // namespace fix_proxy
}  // namespace roq
