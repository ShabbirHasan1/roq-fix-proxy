/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/fix_proxy/client/fix/session.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace fix_proxy {
namespace client {
namespace fix {

// === HELPERS ===

namespace {}  // namespace

// === IMPLEMENTATION ===

Session::Session(
    client::Session::Handler &handler, uint64_t session_id, io::net::tcp::Connection::Factory &factory, Shared &shared)
    : handler_{handler}, session_id_{session_id}, connection_{factory.create(*this)}, shared_{shared} {
}

void Session::operator()(Trace<fix_bridge::fix::BusinessMessageReject> const &event) {
  if (zombie())
    return;
  /*
  auto &[trace_info, business_message_reject] = event;
  send_text(
      R"({{)"
      R"("jsonrpc":"{}",)"
      R"("method":"business_message_reject",)"
      R"("params":{})"
      R"(}})"sv,
      JSONRPC_VERSION,
      json::BusinessMessageReject{business_message_reject});
  */
}

void Session::operator()(Trace<fix_bridge::fix::OrderCancelReject> const &) {
  // XXX TODO send notification
}

void Session::operator()(Trace<fix_bridge::fix::ExecutionReport> const &event) {
  if (zombie())
    return;
  /*
  auto &[trace_info, execution_report] = event;
  send_text(
      R"({{)"
      R"("jsonrpc":"{}",)"
      R"("method":"execution_report",)"
      R"("params":{})"
      R"(}})"sv,
      JSONRPC_VERSION,
      json::ExecutionReport{execution_report});
  */
}

bool Session::ready() const {
  return state_ == State::READY;
}

bool Session::zombie() const {
  return state_ == State::ZOMBIE;
}

void Session::close() {
  state_ = State::ZOMBIE;
  (*connection_).close();
}

// io::net::tcp::Connection::Handler

void Session::operator()(io::net::tcp::Connection::Read const &) {
}

void Session::operator()(io::net::tcp::Connection::Disconnected const &) {
}

}  // namespace fix
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
