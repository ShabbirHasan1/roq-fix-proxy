/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_set.h>

#include <fmt/compile.h>
#include <fmt/format.h>

#include <string>
#include <string_view>

namespace roq {
namespace fix_proxy {

struct Config final {
  static Config parse_file(std::string_view const &);
  static Config parse_text(std::string_view const &);

  absl::flat_hash_set<std::string> const symbols;

 protected:
  explicit Config(auto &node);
};

}  // namespace fix_proxy
}  // namespace roq

template <>
struct fmt::formatter<roq::fix_proxy::Config> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::fix_proxy::Config const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(symbols=[{}])"
        R"(}})"_cf,
        fmt::join(value.symbols, ", "sv));
  }
};
