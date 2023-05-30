/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/settings.hpp"

#include "simple/flags/flags.hpp"

namespace simple {

// === IMPLEMENTATION ===

Settings Settings::create() {
  return {
      .config_file = flags::Flags::config_file(),
      .port = flags::Flags::port(),
  };
}

}  // namespace simple
