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
auto create_fix_session(auto &handler, auto &settings, auto &context, auto &shared, auto &connections) {
  if (std::size(connections) != 1)
    roq::log::fatal("Unexpected: only supporting 1 FIX connection (for now)"sv);
  auto &connection = connections[0];
  auto uri = roq::io::web::URI{connection};
  roq::log::debug("{}"sv, uri);
  return std::make_unique<fix::Session>(handler, settings, context, shared, uri);
}

auto create_json_listener(auto &handler, auto &settings, auto &context) {
  auto network_address = roq::io::NetworkAddress{settings.json.listen_port};
  roq::log::debug("{}"sv, network_address);
  return context.create_tcp_listener(handler, network_address);
}
}  // namespace

// === IMPLEMENTATION ===

Controller::Controller(
    Settings const &settings,
    Config const &config,
    roq::io::Context &context,
    std::span<std::string_view> const &connections)
    : context_{context}, terminate_{context.create_signal(*this, roq::io::sys::Signal::Type::TERMINATE)},
      interrupt_{context.create_signal(*this, roq::io::sys::Signal::Type::INTERRUPT)},
      timer_{context.create_timer(*this, TIMER_FREQUENCY)}, shared_{settings, config},
      fix_session_{create_fix_session(*this, settings, context, shared_, connections)},
      json_listener_{create_json_listener(*this, settings, context_)} {
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
  auto session = std::make_unique<json::Session>(*this, session_id, factory, shared_);
  json_sessions_.try_emplace(session_id, std::move(session));
}

// fix::Session::Handler

void Controller::operator()(roq::Trace<roq::fix_bridge::fix::BusinessMessageReject> const &) {
  roq::log::info("DEBUG not implemented"sv);
}

void Controller::operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReject> const &) {
  roq::log::info("DEBUG not implemented"sv);
}

void Controller::operator()(roq::Trace<roq::fix_bridge::fix::ExecutionReport> const &) {
  roq::log::info("DEBUG not implemented"sv);
}

// json::Handler

void Controller::operator()(roq::Trace<roq::fix_bridge::fix::NewOrderSingle> const &event) {
  (*fix_session_)(event);
}

void Controller::operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReplaceRequest> const &event) {
  (*fix_session_)(event);
}

void Controller::operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelRequest> const &event) {
  (*fix_session_)(event);
}

// utilities

void Controller::remove_zombies(std::chrono::nanoseconds now) {
  if (now < next_garbage_collection_)
    return;
  next_garbage_collection_ = now + 1s;
  for (auto session_id : shared_.sessions_to_remove) {
    roq::log::info("Removing session_id={}..."sv, session_id);
    json_sessions_.erase(session_id);
  }
  shared_.sessions_to_remove.clear();
}

template <typename... Args>
void Controller::dispatch(Args &&...args) {
  auto message_info = roq::MessageInfo{};
  roq::Event event{message_info, std::forward<Args>(args)...};
  (*fix_session_)(event);
}

}  // namespace simple
