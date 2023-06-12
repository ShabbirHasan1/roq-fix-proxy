/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/config.hpp"

#include <toml++/toml.h>

#include "roq/exceptions.hpp"
#include "roq/logging.hpp"

using namespace std::literals;

namespace simple {

// === HELPERS ===

namespace {
template <typename R>
void parse_symbols(R &result, auto &node) {
  using value_type = typename R::value_type;
  if (node.is_value()) {
    result.emplace(*node.template value<value_type>());
  } else if (node.is_array()) {
    auto &arr = *node.as_array();
    for (auto &node_2 : arr) {
      result.emplace(*node_2.template value<value_type>());
    }
  } else {
    throw roq::RuntimeError{"Unexpected"sv};
  }
}

auto parse(auto &root) {
  Config result;
  auto table = root.as_table();
  for (auto &[key, value] : *table) {
    if (key == "symbols"sv) {
      parse_symbols(result.symbols, value);
    } else {
      throw roq::RuntimeError{R"(Unexpected: key="{}")"sv, key};
    };
  }
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Config Config::parse_file(std::string_view const &path) {
  auto root = toml::parse_file(path);
  return parse(root);
}

Config Config::parse_text(std::string_view const &text) {
  auto root = toml::parse(text);
  return parse(root);
}

}  // namespace simple
