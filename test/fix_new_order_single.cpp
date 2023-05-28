/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_test_macros.hpp>

#include "roq/debug/fix/message.hpp"
#include "roq/debug/hex/message.hpp"

#include "roq/fix_bridge/fix/new_order_single.hpp"

using namespace std::literals;
using namespace std::chrono_literals;

namespace {
using Header = roq::fix::Header;
using NewOrderSingle = roq::fix_bridge::fix::NewOrderSingle;
constexpr auto NaN = std::numeric_limits<double>::quiet_NaN();
};  // namespace

TEST_CASE("fix_new_order_single", "[fix_new_order_single]") {
  std::vector<std::byte> buffer(4096);
  auto new_order_single = NewOrderSingle{
      .cl_ord_id = "123"sv,
      .no_party_ids = {},
      .account = "A1",
      .handl_inst = {},
      .exec_inst = {},
      .no_trading_sessions = {},
      .symbol = "BTC-PERPETUAL"sv,
      .security_exchange = "deribit"sv,
      .side = roq::fix::Side::BUY,
      .transact_time = 1685248384123ms,
      .order_qty = 1.0,
      .ord_type = roq::fix::OrdType::LIMIT,
      .price = 27193.0,
      .stop_px = NaN,
      .time_in_force = roq::fix::TimeInForce::GTC,
      .text = {},
      .position_effect = {},
      .max_show = NaN,
  };
  auto header = Header{
      .version = roq::fix::Version::FIX_44,
      .msg_type = new_order_single.msg_type,
      .sender_comp_id = "sender"sv,
      .target_comp_id = "target"sv,
      .msg_seq_num = 1,
      .sending_time = 1685248384123ms,
  };
  auto message = new_order_single.encode(header, buffer);
  REQUIRE(std::size(message) > 0);
  auto tmp = fmt::format("{}"sv, roq::debug::fix::Message{message});
  // note! you can use https://fixparser.targetcompid.com/ to decode this message
  auto expected =
      "8=FIX.4.4|9=0000176|35=D|49=sender|56=target|34=1|52=20230528-04:33:04.123|"
      "11=123|1=A1|55=BTC-PERPETUAL|207=deribit|54=1|60=20230528-04:33:04.123|"
      "38=1.000000000000|40=2|44=27193.000000000000|59=1|"
      "10=201|"sv;
  CHECK(tmp == expected);
  fmt::print(stderr, "{}\n"sv, roq::debug::hex::Message{message});
}
