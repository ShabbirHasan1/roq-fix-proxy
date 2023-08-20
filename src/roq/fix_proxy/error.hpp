/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <string_view>

namespace roq {
namespace fix_proxy {

struct Error final {
  static std::string_view const NOT_READY;
  static std::string_view const SUCCESS;
  static std::string_view const NOT_LOGGED_ON;
  static std::string_view const ALREADY_LOGGED_ON;
};

}  // namespace fix_proxy
}  // namespace roq
