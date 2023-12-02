/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include <catch2/catch_test_macros.hpp>

#include "roq/proxy/fix/tools/crypto.hpp"

using namespace std::literals;

using namespace roq::proxy::fix;

TEST_CASE("proxy_tools_crypto_simple", "[fix_proxy_tools_crypto]") {
  tools::Crypto crypto{true};
  auto res_1 = crypto.validate("foobar"sv, "foobar"sv, {});
  CHECK(res_1 == true);
  auto res_2 = crypto.validate("foobar"sv, "123456"sv, {});
  CHECK(res_2 == false);
}

TEST_CASE("proxy_tools_crypto_hmac_sha256", "[fix_proxy_tools_crypto]") {
  tools::Crypto crypto{false};
  auto res_1 = crypto.validate("foobar"sv, "foobar"sv, {});
  CHECK(res_1 == false);
  auto res_2 = crypto.validate("foobar"sv, "foobar"sv, "abc123"sv);
  CHECK(res_2 == true);
}
