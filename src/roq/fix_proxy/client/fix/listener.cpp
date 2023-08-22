/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/fix_proxy/client/fix/listener.hpp"

#include "roq/logging.hpp"

#include "roq/fix_proxy/client/fix/session.hpp"

using namespace std::literals;

namespace roq {
namespace fix_proxy {
namespace client {
namespace fix {

// === HELPERS ===

namespace {
auto create_listener(auto &handler, auto &settings, auto &context) {
  if (std::empty(settings.client.listen_address))
    return std::unique_ptr<io::net::tcp::Listener>();
  auto network_address = io::NetworkAddress{settings.client.listen_address};
  log::debug("network_address={}"sv, network_address);
  return context.create_tcp_listener(handler, network_address);
}
}  // namespace

// === IMPLEMENTATION ===

Listener::Listener(Handler &handler, Settings const &settings, io::Context &context)
    : handler_{handler}, listener_{create_listener(*this, settings, context)} {
}

// io::net::tcp::Listener::Handler

void Listener::operator()(io::net::tcp::Connection::Factory &factory) {
  struct Bridge final : public client::Factory {
    Bridge(io::net::tcp::Connection::Factory &factory) : factory_{factory} {}

   protected:
    std::unique_ptr<client::Session> create(
        client::Session::Handler &handler, uint64_t session_id, Shared &shared) override {
      return std::make_unique<Session>(handler, session_id, factory_, shared);
    }

   private:
    io::net::tcp::Connection::Factory &factory_;
  } bridge{factory};
  handler_(bridge);
}

}  // namespace fix
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
