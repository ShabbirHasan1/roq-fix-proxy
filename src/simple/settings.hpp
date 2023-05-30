/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

namespace simple {

struct Settings final {
  static Settings create();

  std::string_view config_file;
  uint16_t port = {};
};

}  // namespace simple
