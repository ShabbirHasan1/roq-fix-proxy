/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

#include <fmt/compile.h>
#include <fmt/format.h>

#include <ranges>

#include <string>
#include <string_view>

namespace roq {
namespace proxy {
namespace fix {

struct User final {
  std::string component;
  std::string username;
  std::string password;
  std::string accounts;  // XXX TODO
  uint32_t strategy_id = {};
};

struct Config final {
  static Config parse_file(std::string_view const &);
  static Config parse_text(std::string_view const &);

  absl::flat_hash_set<std::string> const symbols;
  absl::flat_hash_map<std::string, User> const users;

 protected:
  explicit Config(auto &node);
};

}  // namespace fix
}  // namespace proxy
}  // namespace roq

template <>
struct fmt::formatter<roq::proxy::fix::User> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::proxy::fix::User const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(component="{}", )"
        R"(username="{}", )"
        R"(password="{}", )"
        R"(accounts="{}", )"
        R"(strategy_id={})"
        R"(}})"_cf,
        value.component,
        value.username,
        value.password,
        value.accounts,
        value.strategy_id);
  }
};

template <>
struct fmt::formatter<roq::proxy::fix::Config> {
  template <typename Context>
  constexpr auto parse(Context &context) {
    return std::begin(context);
  }
  template <typename Context>
  auto format(roq::proxy::fix::Config const &value, Context &context) const {
    using namespace std::literals;
    using namespace fmt::literals;
    return fmt::format_to(
        context.out(),
        R"({{)"
        R"(symbols=[{}], )"
        R"(users=[{}])"
        R"(}})"_cf,
        fmt::join(value.symbols, ", "sv),
        fmt::join(std::ranges::views::transform(value.users, [this](auto &item) { return item.second; }), ","sv));
  }
};
