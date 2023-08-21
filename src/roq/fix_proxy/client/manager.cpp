/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/fix_proxy/client/manager.hpp"

#include "roq/event.hpp"
#include "roq/exceptions.hpp"
#include "roq/timer.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace fix_proxy {
namespace client {

// === IMPLEMENTATION ===

Manager::Manager(Session::Handler &handler, Settings const &settings, roq::io::Context &context, Shared &shared)
    : handler_{handler}, fix_listener_{*this, settings, context}, json_listener_{*this, settings, context},
      shared_{shared} {
}

void Manager::operator()(Timer const &event) {
  // dispatch(timer);
  remove_zombies(event.now);
}

// json::Listener::Handler

void Manager::operator()(Factory &factory) {
  auto session_id = ++shared_.next_session_id;
  roq::log::info("Adding session_id={}..."sv, session_id);
  auto session = factory.create(handler_, session_id, shared_);
  sessions_.try_emplace(session_id, std::move(session));
}

// utilities

void Manager::remove_zombies(std::chrono::nanoseconds now) {
  if (now < next_garbage_collection_)
    return;
  next_garbage_collection_ = now + 1s;
  shared_.session_cleanup([&](auto session_id) { sessions_.erase(session_id); });
}

/*
template <typename... Args>
void Controller::dispatch(Args &&...args) {
  auto message_info = roq::MessageInfo{};
  roq::Event event{message_info, std::forward<Args>(args)...};
  for (auto &[_, item] : server_sessions_)
    (*item)(event);
}

template <typename T>
void Controller::dispatch_to_fix(roq::Trace<T> const &event, std::string_view const &username) {
  auto iter = server_sessions_.find(username);
  if (iter == std::end(server_sessions_)) [[unlikely]]
    roq::log::fatal(R"(Unexpected: username="{}")"sv, username);  // note! should not be possible
  (*(*iter).second)(event);
}

template <typename T>
void Controller::dispatch_to_json(roq::Trace<T> const &event, std::string_view const &username) {
  auto success = false;
  shared_.session_find(username, [&](auto session_id) {
    auto iter = client_sessions_.find(session_id);
    if (iter != std::end(client_sessions_)) {
      (*(*iter).second)(event);
      success = true;
    }
  });
  if (!success)
    roq::log::warn<0>(R"(Undeliverable: username="{}")"sv);
}
*/

}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
