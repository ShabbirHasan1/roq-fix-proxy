/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/shared.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace simple {

// === CONSTANTS ===

namespace {
auto const GET_SYMBOLS = "symbols"sv;
}

// === HELPERS ===

namespace {
template <typename R>
auto create_regex_symbols(auto &config) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &symbol : config.symbols) {
    third_party::re2::RegularExpression regular_expression{symbol};
    result.emplace_back(std::move(regular_expression));
  }
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Shared::Shared(Settings const &settings, Config const &config)
    : settings{settings}, regex_symbols_{create_regex_symbols<decltype(regex_symbols_)>(config)} {
}

bool Shared::include(std::string_view const &symbol) const {
  for (auto &regex : regex_symbols_)
    if (regex.match(symbol))
      return true;
  return false;
}

}  // namespace simple
