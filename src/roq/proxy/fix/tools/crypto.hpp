/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

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

 private:
  bool const simple_;
  std::array<std::byte, MAC::DIGEST_LENGTH> digest_;
};

}  // namespace tools
}  // namespace fix
}  // namespace proxy
}  // namespace roq
