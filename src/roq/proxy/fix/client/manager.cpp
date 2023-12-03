/* Copyright (c) 2017-2024, Hans Erik Thrane */

#include "roq/proxy/fix/client/manager.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace proxy {
namespace fix {
namespace client {

// === CONSTANTS ===

namespace {
auto const GARBAGE_COLLECTION_FREQUENCY = 1s;
}

// === IMPLEMENTATION ===

Manager::Manager(Session::Handler &handler, Settings const &settings, io::Context &context, Shared &shared)
    : handler_{handler}, fix_listener_{*this, settings, context}, shared_{shared} {
}

void Manager::operator()(Event<Start> const &) {
}

void Manager::operator()(Event<Stop> const &event) {
  dispatch(event);
}

void Manager::operator()(Event<Timer> const &event) {
  dispatch(event);
  remove_zombies(event.value.now);
}

// fix::Listener::Handler

void Manager::operator()(Factory &factory) {
  auto session_id = ++shared_.next_session_id;
  log::info("Adding session_id={}..."sv, session_id);
  auto session = factory.create(handler_, session_id, shared_);
  sessions_.try_emplace(session_id, std::move(session));
}

// utilities

void Manager::remove_zombies(std::chrono::nanoseconds now) {
  if (now < next_garbage_collection_)
    return;
  next_garbage_collection_ = now + GARBAGE_COLLECTION_FREQUENCY;
  shared_.session_cleanup([&](auto session_id) { sessions_.erase(session_id); });
}

}  // namespace client
}  // namespace fix
}  // namespace proxy
}  // namespace roq
