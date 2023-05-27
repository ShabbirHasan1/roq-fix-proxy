/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_test_macros.hpp>

// note! the NewOrderSingle template doesn't yet create 2-way bindings

#include "roq/fix/new_order_single.hpp"

// #include "roq/fix_bridge/fix/new_order_single.hpp"
#include "roq/fix_bridge/fix/reject.hpp"

using namespace std::literals;
using namespace std::chrono_literals;

using Header = roq::fix::Header;
using Reject = roq::fix_bridge::fix::Reject;

TEST_CASE("fix_new_order_single", "[fix_new_order_single]") {
  std::vector<std::byte> buffer(4096);
  auto reject = Reject{
      .ref_seq_num = 1,
      .text = "failure"sv,
      .ref_tag_id = {},
      .ref_msg_type = roq::fix::NewOrderSingle::msg_type,
      .session_reject_reason = roq::fix::SessionRejectReason ::INVALID_MSG_TYPE,
  };
  auto header = Header{
      .version = roq::fix::Version::FIX_44,
      .msg_type = reject.msg_type,
      .sender_comp_id = "sender"sv,
      .target_comp_id = "target"sv,
      .msg_seq_num = 1,
      .sending_time = 1685157508123ms,
  };
  auto message = reject.encode(header, buffer);
  CHECK(std::size(message) > 0);
}
