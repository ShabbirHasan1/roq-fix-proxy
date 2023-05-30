/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/fix/session.hpp"

#include "roq/logging.hpp"

#include "roq/debug/fix/message.hpp"
#include "roq/debug/hex/message.hpp"

#include "roq/fix/reader.hpp"

#include "roq/fix_bridge/fix/heartbeat.hpp"

using namespace std::literals;

namespace simple {
namespace fix {

// === CONSTANTS ===

namespace {
auto const FIX_VERSION = roq::fix::Version::FIX_44;
}

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
      connection_manager_{create_connection_manager(*this, settings, *connection_factory_)},
      debug_{settings.fix.debug} {
}

void Session::operator()(roq::Event<roq::Start> const &) {
  (*connection_manager_).start();
}

void Session::operator()(roq::Event<roq::Stop> const &) {
  (*connection_manager_).stop();
}

void Session::operator()(roq::Event<roq::Timer> const &event) {
  (*connection_manager_).refresh(event.value.now);
}

// io::net::ConnectionManager::Handler

void Session::operator()(roq::io::net::ConnectionManager::Connected const &) {
  roq::log::debug("Connected"sv);
}

void Session::operator()(roq::io::net::ConnectionManager::Disconnected const &) {
  roq::log::debug("Disconnected"sv);
}

void Session::operator()(roq::io::net::ConnectionManager::Read const &) {
  auto buffer = (*connection_manager_).buffer();
  size_t total_bytes = 0;
  while (!std::empty(buffer)) {
    roq::TraceInfo trace_info;
    auto bytes = roq::fix::Reader<FIX_VERSION>::dispatch(
        buffer,
        [&](roq::fix::Message const &message) {
          try {
            check(message.header);
            roq::Trace event{trace_info, message};
            parse(event);
          } catch (std::exception &) {
            roq::log::warn("{}"sv, roq::debug::fix::Message{buffer});
#ifndef NDEBUG
            roq::log::warn("{}"sv, roq::debug::hex::Message{buffer});
#endif
            roq::log::error("Message could not be parsed. PLEASE REPORT!"sv);
          }
        },
        [this](auto &message) {
          if (debug_)
            roq::log::info("{}"sv, roq::debug::fix::Message{message});
        });
    if (bytes == 0)
      break;
    assert(bytes <= std::size(buffer));
    total_bytes += bytes;
    buffer = buffer.subspan(bytes);
  }
  (*connection_manager_).drain(total_bytes);
}

void Session::check(roq::fix::Header const &header) {
  auto current = header.msg_seq_num;
  auto expected = inbound_.msg_seq_num + 1;
  if (current != expected) [[unlikely]] {
    if (expected < current) {
      roq::log::warn(
          "*** SEQUENCE GAP *** "
          "current={} previous={} distance={}"sv,
          current,
          inbound_.msg_seq_num,
          current - inbound_.msg_seq_num);
    } else {
      roq::log::warn(
          "*** SEQUENCE REPLAY *** "
          "current={} previous={} distance={}"sv,
          current,
          inbound_.msg_seq_num,
          inbound_.msg_seq_num - current);
    }
  }
  inbound_.msg_seq_num = current;
}

void Session::parse(roq::Trace<roq::fix::Message> const &event) {
  auto &[trace_info, message] = event;
  switch (message.header.msg_type) {
    using enum roq::fix::MsgType;
    case HEARTBEAT: {
      auto heartbeat = roq::fix_bridge::fix::Heartbeat::create(message);
      roq::Trace trace{trace_info, heartbeat};
      (*this)(trace, message.header);
      return;
    }
    default:
      break;
  }
  roq::log::warn("Unexpected msg_type={}"sv, message.header.msg_type);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::Heartbeat> const &, roq::fix::Header const &) {
}

}  // namespace fix
}  // namespace simple
