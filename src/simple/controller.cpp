/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/controller.hpp"

#include "roq/exceptions.hpp"
#include "roq/logging.hpp"

#include "tools/simple.hpp"

using namespace std::literals;

using namespace roq;

namespace simple {

// === CONSTANTS ===

namespace {
auto const TIMER_FREQUENCY = 100ms;
}

// === HELPERS ===

namespace {
auto create_listener(auto &handler, auto &settings, auto &context) {
  auto network_address = io::NetworkAddress{settings.port};
  return context.create_tcp_listener(handler, network_address);
}
}  // namespace

// === IMPLEMENTATION ===
//
Controller::Controller(
    Settings const &settings, Config const &, io::Context &context, std::span<std::string_view> const &connections)
    : context_{context}, terminate_{context.create_signal(*this, roq::io::sys::Signal::Type::TERMINATE)},
      interrupt_{context.create_signal(*this, roq::io::sys::Signal::Type::INTERRUPT)},
      timer_{context.create_timer(*this, TIMER_FREQUENCY)}, listener_{create_listener(*this, settings, context_)} {
}

void Controller::run() {
  log::info("Event loop is now running"sv);
  context_.dispatch();
  log::info("Event loop has terminated"sv);
}

// io::sys::Signal::Handler

void Controller::operator()(io::sys::Signal::Event const &event) {
  log::warn("*** SIGNAL: {} ***"sv, magic_enum::enum_name(event.type));
  context_.stop();
}

// io::sys::Timer::Handler

void Controller::operator()(io::sys::Timer::Event const &event) {
  remove_zombies(event.now);
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
