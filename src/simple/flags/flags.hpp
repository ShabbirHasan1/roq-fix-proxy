/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <chrono>
#include <string_view>

namespace simple {
namespace flags {

struct Flags final {
  static std::string_view config_file();
  static uint16_t listen_port();
  static std::string_view fix_sender_comp_id();
  static std::string_view fix_target_comp_id();
  static uint32_t fix_decode_buffer_size();
  static uint32_t fix_encode_buffer_size();
  static std::chrono::seconds fix_ping_freq();
  static bool fix_debug();
};

}  // namespace flags
}  // namespace simple
