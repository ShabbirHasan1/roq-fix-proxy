/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string_view>

#include "roq/args/parser.hpp"

#include "simple/flags/fix.hpp"
#include "simple/flags/flags.hpp"
#include "simple/flags/rest.hpp"

namespace simple {

struct Settings final {
  // note! dependency on roq::args::Parser to enforce correct sequencing
  static Settings create(roq::args::Parser const &);

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

}  // namespace simple
