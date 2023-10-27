/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/shared.hpp"

#include "roq/logging.hpp"

#include "roq/clock.hpp"

#include "roq/proxy/fix/error.hpp"

using namespace std::literals;

namespace roq {
namespace proxy {
namespace fix {

// === HELPERS ===

namespace {
template <typename R>
auto create_username_to_password_and_strategy_id(auto &config) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &[_, user] : config.users)
    result.try_emplace(user.username, user.password, user.strategy_id);
  return result;
}

template <typename R>
auto create_regex_symbols(auto &config) {
  using result_type = std::remove_cvref<R>::type;
  result_type result;
  for (auto &symbol : config.symbols) {
    utils::regex::Pattern regex{symbol};
    result.emplace_back(std::move(regex));
  }
  return result;
}

auto create_next_request_id() {
  return static_cast<uint64_t>(clock::get_realtime().count());
}
}  // namespace

// === IMPLEMENTATION ===

Shared::Shared(Settings const &settings, Config const &config)
    : settings{settings},
      username_to_password_and_strategy_id_{
          create_username_to_password_and_strategy_id<decltype(username_to_password_and_strategy_id_)>(config)},
      regex_symbols_{create_regex_symbols<decltype(regex_symbols_)>(config)},
      next_request_id_{create_next_request_id()} {
}

bool Shared::include(std::string_view const &symbol) const {
  for (auto &regex : regex_symbols_)
    if (regex.match(symbol))
      return true;
  return false;
}

void Shared::session_remove(uint64_t session_id) {
  sessions_to_remove_.emplace(session_id);
  session_remove_helper(session_id);
}

std::string_view Shared::session_logon_helper(
    uint64_t session_id, std::string_view const &username, std::string_view const &password, uint32_t &strategy_id) {
  auto iter_1 = username_to_password_and_strategy_id_.find(username);
  if (iter_1 == std::end(username_to_password_and_strategy_id_) || password != (*iter_1).second.first) {
    log::warn("Invalid: password"sv);
    return Error::INVALID_PASSWORD;
  }
  auto iter_2 = username_to_session_.find(username);
  if (iter_2 != std::end(username_to_session_)) {
    log::warn(R"(Invalid: user already logged on (check session_id={}, username="{}"))"sv, (*iter_2).second, username);
    return Error::ALREADY_LOGGED_ON;
  }
  log::info(R"(Adding session_id={}, username="{}")"sv, session_id, username);
  auto res_1 = username_to_session_.try_emplace(username, session_id);
  assert(res_1.second);
  auto &username_1 = (*res_1.first).first;
  auto res_2 = session_to_username_.try_emplace(session_id, username_1);
  assert(res_2.second);
  strategy_id = (*iter_1).second.second;
  return {};
}

std::string_view Shared::session_logout_helper(uint64_t session_id) {
  auto iter = session_to_username_.find(session_id);
  if (iter == std::end(session_to_username_))
    return Error::NOT_LOGGED_ON;
  auto &username = (*iter).second;
  log::info(R"(Removing session_id={}, username="{}")"sv, session_id, username);
  username_to_session_.erase(username);
  session_to_username_.erase(iter);
  return {};
}

void Shared::session_remove_helper(uint64_t session_id) {
  auto iter = session_to_username_.find(session_id);
  if (iter != std::end(session_to_username_)) {
    auto &username = (*iter).second;
    log::info(R"(Removing session_id={}, username="{}")"sv, session_id, username);
    username_to_session_.erase((*iter).second);
    session_to_username_.erase(iter);
  }
}

void Shared::session_cleanup_helper(uint64_t session_id) {
  session_remove_helper(session_id);
  log::info("Removing session_id={}..."sv, session_id);
}

std::string Shared::create_request_id() {
  return fmt::format("proxy-{}"sv, ++next_request_id_);
}

}  // namespace fix
}  // namespace proxy
}  // namespace roq
