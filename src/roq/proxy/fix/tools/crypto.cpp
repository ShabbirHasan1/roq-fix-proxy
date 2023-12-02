/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/tools/crypto.hpp"

#include "roq/logging.hpp"

#include "roq/utils/codec/base64.hpp"

using namespace std::literals;

namespace roq {
namespace proxy {
namespace fix {
namespace tools {

// === IMPLEMENTATION ===

Crypto::Crypto(bool simple) : simple_{simple} {
}

bool Crypto::validate(
    std::string_view const &password, std::string_view const &secret, std::string_view const &raw_data) {
  if (simple_)
    return password == secret;
  MAC mac{secret};  // alloc
  // mac.clear();
  mac.update(raw_data);
  auto digest = mac.final(digest_);
  std::string result;
  utils::codec::Base64::encode(result, digest, false, false);  // alloc
  return result == password;
}

}  // namespace tools
}  // namespace fix
}  // namespace proxy
}  // namespace roq
