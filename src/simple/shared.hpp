/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <string>
#include <vector>

#include "simple/config.hpp"

#include "third_party/re2/regular_expression.hpp"

#include "simple/settings.hpp"

namespace simple {

struct Shared final {
  Shared(Settings const &, Config const &);

  absl::flat_hash_set<uint64_t> sessions_to_remove;
  absl::flat_hash_set<std::string> symbols;

  bool include(std::string_view const &symbol) const;

  Settings const &settings;

  std::string encode_buffer;

 private:
  std::vector<third_party::re2::RegularExpression> const regex_symbols_;
};

}  // namespace simple
