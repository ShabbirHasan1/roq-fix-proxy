/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_test_macros.hpp>

#include "roq/debug/fix/message.hpp"
#include "roq/debug/hex/message.hpp"

#include "roq/fix/new_order_single.hpp"

// note! the FIX Bridge NewOrderSingle template doesn't yet create 2-way bindings
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
      .session_reject_reason = roq::fix::SessionRejectReason::INVALID_MSG_TYPE,
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
  REQUIRE(std::size(message) > 0);
  auto tmp = fmt::format("{}"sv, roq::debug::fix::Message{message});
  // note! you can use https://fixparser.targetcompid.com/ to decode this message
  auto expected =
      "8=FIX.4.4|9=0000084|35=3|49=sender|56=target|34=1|52=20230527-03:18:28.123|45=1|58=failure|372=D|373=11|10=154|"sv;
  CHECK(tmp == expected);
  fmt::print(stderr, "{}\n"sv, roq::debug::hex::Message{message});
}
