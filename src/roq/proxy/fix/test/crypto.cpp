/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include <catch2/catch_test_macros.hpp>

#include "roq/proxy/fix/tools/crypto.hpp"

using namespace std::literals;

using namespace roq::proxy::fix;

TEST_CASE("proxy_tools_crypto_simple", "[fix_proxy_tools_crypto]") {
  tools::Crypto crypto{""sv};
  auto res_1 = crypto.validate("foobar"sv, "foobar"sv, {});
  CHECK(res_1 == true);
  auto res_2 = crypto.validate("foobar"sv, "123456"sv, {});
  CHECK(res_2 == false);
}

TEST_CASE("proxy_tools_crypto_hmac_sha256", "[fix_proxy_tools_crypto]") {
  tools::Crypto crypto{"hmac_sha256"sv};
  auto res_1 = crypto.validate("foobar"sv, "foobar"sv, {});
  CHECK(res_1 == false);
  auto res_2 = crypto.validate("qEBeeU/7jdamNNZI+b4LBGRrX39qVIc20pPcZY8m5Zg="sv, "foobar"sv, "1234567890"sv);
  CHECK(res_2 == true);
}
