/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/config.hpp"

#include <toml++/toml.h>

#include "roq/logging.hpp"

using namespace std::literals;

namespace simple {

// === HELPERS ===

namespace {
auto parse(auto &root) {
  Config config;
  auto table = root.as_table();
  for (auto &[key, value] : *table) {
    roq::log::debug(R"(key="{}")"sv, key);
  }
  return config;
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
