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

}  // namespace tools
}  // namespace fix
}  // namespace proxy
}  // namespace roq
