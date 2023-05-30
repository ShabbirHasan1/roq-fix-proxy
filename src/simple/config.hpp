/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string>

namespace simple {

struct Config final {
  static Config parse_file(std::string_view const &);
  static Config parse_text(std::string_view const &);
};

}  // namespace simple
