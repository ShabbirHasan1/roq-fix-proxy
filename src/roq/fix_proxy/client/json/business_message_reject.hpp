/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include "roq/json/string.hpp"

#include "roq/fix_bridge/fix/business_message_reject.hpp"

namespace roq {
namespace fix_proxy {
namespace client {
namespace json {

// note! supports both rest and websocket

struct BusinessMessageReject final {
  explicit BusinessMessageReject(fix_bridge::fix::BusinessMessageReject const &value) : value_{value} {}

  template <typename Context>
  auto format_to(Context &context) const {
    using namespace std::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"("ref_seq_num":{},)"
        R"("ref_msg_type":"{}",)"
        R"("business_reject_ref_id":{},)"
        R"("business_reject_reason":{},)"
        R"("text":{})"
        R"(}})"sv,
        value_.ref_seq_num,
        fix::Codec<fix::MsgType>::encode(value_.ref_msg_type),
        roq::json::String{value_.business_reject_ref_id},
        roq::json::String{value_.business_reject_reason},
        roq::json::String{value_.text});
  }

 private:
  fix_bridge::fix::BusinessMessageReject const &value_;
};

}  // namespace json
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq

template <>
struct fmt::formatter<roq::fix_proxy::client::json::BusinessMessageReject> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::fix_proxy::client::json::BusinessMessageReject const &value, Context &context) const {
    return value.format_to(context);
  }
};
