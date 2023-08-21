/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/fix_proxy/controller.hpp"

#include "roq/event.hpp"
#include "roq/exceptions.hpp"
#include "roq/timer.hpp"

#include "roq/logging.hpp"

#include "roq/fix_proxy/client/json/session.hpp"

using namespace std::literals;

namespace roq {
namespace fix_proxy {

// === CONSTANTS ===

namespace {
auto const TIMER_FREQUENCY = 100ms;
}

// === HELPERS ===

namespace {
auto create_server_sessions(auto &handler, auto &settings, auto &context, auto &shared, auto &connections) {
  if (std::size(connections) != 1)
    log::fatal("Unexpected: only supporting 1 FIX connection (for now)"sv);
  absl::flat_hash_map<std::string, std::unique_ptr<server::Session>> result;
  auto &connection = connections[0];
  auto uri = io::web::URI{connection};
  log::debug("{}"sv, uri);
  auto session = std::make_unique<server::Session>(
      handler, settings, context, shared, uri, settings.fix.username, settings.fix.password);
  result.emplace(settings.fix.username, std::move(session));
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Controller::Controller(
    Settings const &settings,
    Config const &config,
    io::Context &context,
    std::span<std::string_view const> const &connections)
    : context_{context}, terminate_{context.create_signal(*this, io::sys::Signal::Type::TERMINATE)},
      interrupt_{context.create_signal(*this, io::sys::Signal::Type::INTERRUPT)},
      timer_{context.create_timer(*this, TIMER_FREQUENCY)}, shared_{settings, config},
      server_sessions_{create_server_sessions(*this, settings, context, shared_, connections)},
      client_manager_{*this, settings, context, shared_} {
}

void Controller::run() {
  log::info("Event loop is now running"sv);
  auto start = Start{};
  dispatch(start);
  (*timer_).resume();
  context_.dispatch();
  auto stop = Stop{};
  dispatch(stop);
  log::info("Event loop has terminated"sv);
}

// io::sys::Signal::Handler

void Controller::operator()(io::sys::Signal::Event const &event) {
  log::warn("*** SIGNAL: {} ***"sv, magic_enum::enum_name(event.type));
  context_.stop();
}

// io::sys::Timer::Handler

void Controller::operator()(io::sys::Timer::Event const &event) {
  auto timer = Timer{
      .now = event.now,
  };
  dispatch(timer);
  client_manager_(timer);
}

// fix::Session::Handler

void Controller::operator()(Trace<fix_bridge::fix::SecurityDefinition> const &event) {
  auto &[trace_info, security_definition] = event;
  shared_.symbols.emplace(security_definition.symbol);  // XXX TODO cache reference data
}

void Controller::operator()(
    Trace<fix_bridge::fix::BusinessMessageReject> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<fix_bridge::fix::OrderCancelReject> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<fix_bridge::fix::ExecutionReport> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

// rest::Session::Handler

void Controller::operator()(Trace<fix_bridge::fix::OrderStatusRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<fix_bridge::fix::NewOrderSingle> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(
    Trace<fix_bridge::fix::OrderCancelReplaceRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<fix_bridge::fix::OrderCancelRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(
    Trace<fix_bridge::fix::OrderMassStatusRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(
    Trace<fix_bridge::fix::OrderMassCancelRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

// utilities

template <typename... Args>
void Controller::dispatch(Args &&...args) {
  auto message_info = MessageInfo{};
  Event event{message_info, std::forward<Args>(args)...};
  for (auto &[_, item] : server_sessions_)
    (*item)(event);
}

template <typename T>
void Controller::dispatch_to_server(Trace<T> const &event, std::string_view const &username) {
  auto iter = server_sessions_.find(username);
  if (iter == std::end(server_sessions_)) [[unlikely]]
    log::fatal(R"(Unexpected: username="{}")"sv, username);  // note! should not be possible
  (*(*iter).second)(event);
}

template <typename T>
void Controller::dispatch_to_client(Trace<T> const &event, std::string_view const &username) {
  auto success = false;
  shared_.session_find(username, [&](auto session_id) {
    client_manager_.find(session_id, [&](auto &session) {
      session(event);
      success = true;
    });
  });
  if (!success)
    log::warn<0>(R"(Undeliverable: username="{}")"sv);
}

}  // namespace fix_proxy
}  // namespace roq
