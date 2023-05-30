/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <string>

namespace simple {

struct Shared final {
  absl::flat_hash_set<uint64_t> sessions_to_remove;
  absl::flat_hash_set<std::string> symbols;
};

}  // namespace simple
