/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/controller.hpp"

#include "roq/event.hpp"
#include "roq/exceptions.hpp"
#include "roq/timer.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace simple {

// === CONSTANTS ===

namespace {
auto const TIMER_FREQUENCY = 100ms;
}

// === HELPERS ===

namespace {
auto create_fix_sessions(auto &settings, auto &context, auto &connections) {
  std::vector<std::unique_ptr<fix::Session>> result;
  for (auto &item : connections) {
    auto uri = roq::io::web::URI{item};
    roq::log::debug("{}"sv, uri);
    auto session = std::make_unique<fix::Session>(settings, context, uri);
    result.emplace_back(std::move(session));
  }
  return result;
}

auto create_web_listener(auto &handler, auto &settings, auto &context) {
  auto network_address = roq::io::NetworkAddress{settings.listen_port};
  roq::log::debug("{}"sv, network_address);
  return context.create_tcp_listener(handler, network_address);
}
}  // namespace

// === IMPLEMENTATION ===

Controller::Controller(
    Settings const &settings, Config const &, roq::io::Context &context, std::span<std::string_view> const &connections)
    : context_{context}, terminate_{context.create_signal(*this, roq::io::sys::Signal::Type::TERMINATE)},
      interrupt_{context.create_signal(*this, roq::io::sys::Signal::Type::INTERRUPT)},
      timer_{context.create_timer(*this, TIMER_FREQUENCY)},
      fix_sessions_{create_fix_sessions(settings, context, connections)},
      web_listener_{create_web_listener(*this, settings, context_)} {
}

void Controller::run() {
  roq::log::info("Event loop is now running"sv);
  auto start = roq::Start{};
  dispatch(start);
  (*timer_).resume();
  context_.dispatch();
  auto stop = roq::Stop{};
  dispatch(stop);
  roq::log::info("Event loop has terminated"sv);
}

// io::sys::Signal::Handler

void Controller::operator()(roq::io::sys::Signal::Event const &event) {
  roq::log::warn("*** SIGNAL: {} ***"sv, magic_enum::enum_name(event.type));
  context_.stop();
}

// io::sys::Timer::Handler

void Controller::operator()(roq::io::sys::Timer::Event const &event) {
  auto timer = roq::Timer{
      .now = event.now,
  };
  dispatch(timer);
  remove_zombies(event.now);
}

// io::net::tcp::Listener::Handler

void Controller::operator()(roq::io::net::tcp::Connection::Factory &factory) {
  auto session_id = ++next_session_id_;
  roq::log::info("Adding session_id={}..."sv, session_id);
  auto session = std::make_unique<web::Session>(session_id, factory, shared_);
  web_sessions_.try_emplace(session_id, std::move(session));
}

// utilities

void Controller::remove_zombies(std::chrono::nanoseconds now) {
  if (now < next_garbage_collection_)
    return;
  next_garbage_collection_ = now + 1s;
  for (auto session_id : shared_.sessions_to_remove) {
    roq::log::info("Removing session_id={}..."sv, session_id);
    web_sessions_.erase(session_id);
  }
  shared_.sessions_to_remove.clear();
}

template <typename... Args>
void Controller::dispatch(Args &&...args) {
  auto message_info = roq::MessageInfo{};
  roq::Event event{message_info, std::forward<Args>(args)...};
  for (auto &item : fix_sessions_)
    (*item)(event);
}

}  // namespace simple
