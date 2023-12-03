/* Copyright (c) 2017-2024, Hans Erik Thrane */

#pragma once

#include <string_view>

#include "roq/utils/hash/sha256.hpp"

#include "roq/utils/mac/hmac.hpp"

namespace roq {
namespace proxy {
namespace fix {
namespace tools {

struct Crypto final {
  using Hash = utils::hash::SHA256;
  using MAC = utils::mac::HMAC<utils::hash::SHA256>;

  explicit Crypto(bool simple);

  Crypto(Crypto &&) = delete;
  Crypto(Crypto const &) = delete;

  bool validate(std::string_view const &password, std::string_view const &secret, std::string_view const &raw_data);

 private:
  bool const simple_;
  std::array<std::byte, MAC::DIGEST_LENGTH> digest_;
};

}  // namespace tools
}  // namespace fix
}  // namespace proxy
}  // namespace roq
