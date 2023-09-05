/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/server/manager.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace proxy {
namespace fix {
namespace server {

// === HELPERS ===

namespace {
auto create_sessions(auto &handler, auto &settings, auto &context, auto &shared, auto &connections) {
  if (std::size(connections) != 1)
    log::fatal("Unexpected: only supporting 1 FIX connection (for now)"sv);
  absl::flat_hash_map<std::string, std::unique_ptr<server::Session>> result;
  auto &connection = connections[0];
  auto uri = io::web::URI{connection};
  log::debug("{}"sv, uri);
  auto session = std::make_unique<server::Session>(
      handler, settings, context, shared, uri, settings.server.username, settings.server.password);
  result.emplace(settings.server.username, std::move(session));
  return result;
}
}  // namespace

// === IMPLEMENTATION ===

Manager::Manager(
    Session::Handler &handler,
    Settings const &settings,
    io::Context &context,
    Shared &shared,
    std::span<std::string_view const> const &connections)
    : sessions_{create_sessions(handler, settings, context, shared, connections)} {
}

void Manager::operator()(Event<Start> const &event) {
  dispatch(event);
}

void Manager::operator()(Event<Stop> const &event) {
  dispatch(event);
}

void Manager::operator()(Event<Timer> const &event) {
  dispatch(event);
}

}  // namespace server
}  // namespace fix
}  // namespace proxy
}  // namespace roq
