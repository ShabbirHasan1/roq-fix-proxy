/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/flags/flags.hpp"

#include <string>

#include <absl/flags/flag.h>

ABSL_FLAG(  //
    std::string,
    config_file,
    {},
    "config file (path)");

ABSL_FLAG(  //
    uint16_t,
    listen_port,
    {},
    "listen port");

ABSL_FLAG(  //
    std::string,
    fix_target_comp_id,
    {},
    "fix target comp id");

ABSL_FLAG(  //
    std::string,
    fix_sender_comp_id,
    {},
    "fix sender comp id");

ABSL_FLAG(  //
    std::string,
    fix_username,
    {},
    "fix username");

ABSL_FLAG(  //
    uint32_t,
    fix_decode_buffer_size,
    1048576,
    "fix deode buffer size");

ABSL_FLAG(  //
    uint32_t,
    fix_encode_buffer_size,
    65536,
    "fix encode buffer size");

// XXX TODO make this absl duration
ABSL_FLAG(  //
    uint32_t,
    fix_ping_freq,
    {30},
    "fix ping freq (seconds)");

ABSL_FLAG(  //
    bool,
    fix_debug,
    false,
    "fix debug?");

namespace simple {
namespace flags {

std::string_view Flags::config_file() {
  static std::string const result = absl::GetFlag(FLAGS_config_file);
  return result;
}

uint16_t Flags::listen_port() {
  static uint16_t const result = absl::GetFlag(FLAGS_listen_port);
  return result;
}

std::string_view Flags::fix_target_comp_id() {
  static std::string const result = absl::GetFlag(FLAGS_fix_target_comp_id);
  return result;
}

std::string_view Flags::fix_sender_comp_id() {
  static std::string const result = absl::GetFlag(FLAGS_fix_sender_comp_id);
  return result;
}

std::string_view Flags::fix_username() {
  static std::string const result = absl::GetFlag(FLAGS_fix_username);
  return result;
}

uint32_t Flags::fix_decode_buffer_size() {
  static uint32_t const result = absl::GetFlag(FLAGS_fix_decode_buffer_size);
  return result;
}

uint32_t Flags::fix_encode_buffer_size() {
  static uint32_t const result = absl::GetFlag(FLAGS_fix_encode_buffer_size);
  return result;
}

std::chrono::seconds Flags::fix_ping_freq() {
  static std::chrono::seconds const result{absl::GetFlag(FLAGS_fix_ping_freq)};
  return result;
}

bool Flags::fix_debug() {
  static bool const result = absl::GetFlag(FLAGS_fix_debug);
  return result;
}

}  // namespace flags
}  // namespace simple
