/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string_view>

namespace simple {

struct Settings final {
  static Settings create();

  std::string_view config_file;
  uint16_t listen_port = {};

  struct {
    std::chrono::nanoseconds connection_timeout = {};
    bool tls_validate_certificate = {};
  } net;

  struct {
    std::string_view sender_comp_id;
    std::string_view target_comp_id;
    size_t encode_buffer_size = {};
    bool debug = {};
  } fix;
};

}  // namespace simple
