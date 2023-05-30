/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/controller.hpp"

#include "roq/logging.hpp"

#include "tools/simple.hpp"

#include "simple/flags/flags.hpp"

using namespace std::literals;

using namespace roq;

namespace simple {

// === HELPERS ===

namespace {
auto create_listener(auto &handler, auto &context) {
  auto network_address = io::NetworkAddress{flags::Flags::port()};
  return context.create_tcp_listener(handler, network_address);
}
}  // namespace

// === IMPLEMENTATION ===
//
Controller::Controller(client::Dispatcher &dispatcher, io::Context &context)
    : dispatcher_{dispatcher}, context_{context}, listener_{create_listener(*this, context_)} {
}

// client::Handler

void Controller::operator()(Event<Timer> const &event) {
  context_.drain();
  remove_zombies(event.value.now);
}

// io::net::tcp::Listener::Handler

void Controller::operator()(io::net::tcp::Connection::Factory &factory) {
  auto session_id = ++next_session_id_;
  log::info("Adding session_id={}..."sv, session_id);
  auto session = std::make_unique<Session>(session_id, factory, shared_);
  sessions_.try_emplace(session_id, std::move(session));
}

// utilities

void Controller::remove_zombies(std::chrono::nanoseconds now) {
  if (now < next_garbage_collection_)
    return;
  next_garbage_collection_ = now + 1s;
  for (auto session_id : shared_.sessions_to_remove) {
    log::info("Removing session_id={}..."sv, session_id);
    sessions_.erase(session_id);
  }
  shared_.sessions_to_remove.clear();
}

}  // namespace simple
