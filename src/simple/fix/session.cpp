/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/fix/session.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace simple {
namespace fix {

// === HELPERS ===

namespace {
auto create_connection_factory(auto &settings, auto &context, auto &uri) {
  auto config = roq::io::net::ConnectionFactory::Config{
      .interface = {},
      .uris = {&uri, 1},
      .validate_certificate = settings.net.tls_validate_certificate,
  };
  return roq::io::net::ConnectionFactory::create(context, config);
}

auto create_connection_manager(auto &handler, auto &settings, auto &connection_factory) {
  auto config = roq::io::net::ConnectionManager::Config{
      .connection_timeout = settings.net.connection_timeout,
      .disconnect_on_idle_timeout = {},
      .always_reconnect = true,
  };
  return roq::io::net::ConnectionManager::create(handler, connection_factory, config);
}
}  // namespace

// === IMPLEMENTATION ===

Session::Session(Settings const &settings, roq::io::Context &context, roq::io::web::URI const &uri)
    : connection_factory_{create_connection_factory(settings, context, uri)},
      connection_manager_{create_connection_manager(*this, settings, *connection_factory_)} {
}

void Session::start() {
  (*connection_manager_).start();
}

void Session::stop() {
  (*connection_manager_).stop();
}

void Session::refresh(std::chrono::nanoseconds now) {
  (*connection_manager_).refresh(now);
}

// io::net::ConnectionManager::Handler

void Session::operator()(roq::io::net::ConnectionManager::Connected const &) {
}

void Session::operator()(roq::io::net::ConnectionManager::Disconnected const &) {
}

void Session::operator()(roq::io::net::ConnectionManager::Read const &) {
}

}  // namespace fix
}  // namespace simple
