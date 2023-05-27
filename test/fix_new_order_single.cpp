/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_test_macros.hpp>

// #include "roq/fix_bridge/fix/new_order_single.hpp"
#include "roq/fix_bridge/fix/reject.hpp"

using Header = roq::fix::Header;
using Reject = roq::fix_bridge::fix::Reject;

TEST_CASE("fix_new_order_single", "[fix_new_order_single]") {
  std::vector<std::byte> buffer(4096);
  auto header = Header{};
  auto reject = Reject{};
  auto message = reject.encode(header, buffer);
}
