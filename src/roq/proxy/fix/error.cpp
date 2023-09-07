/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/error.hpp"

using namespace std::literals;

namespace roq {
namespace proxy {
namespace fix {

// === IMPLEMENTATION ===

std::string_view const Error::NOT_READY = "NOT_READY"sv;
std::string_view const Error::SUCCESS = "SUCCESS"sv;
std::string_view const Error::NOT_LOGGED_ON = "NOT_LOGGED_ON"sv;
std::string_view const Error::ALREADY_LOGGED_ON = "ALREADY_LOGGED_ON"sv;
std::string_view const Error::INVALID_PASSWORD = "INVALID_PASSWORD"sv;

}  // namespace fix
}  // namespace proxy
}  // namespace roq
