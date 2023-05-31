/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/fix/session.hpp"

#include "roq/logging.hpp"

#include "roq/utils/update.hpp"

#include "roq/debug/fix/message.hpp"
#include "roq/debug/hex/message.hpp"

#include "roq/fix/reader.hpp"

using namespace std::literals;

namespace simple {
namespace fix {

// === CONSTANTS ===

namespace {
auto const FIX_VERSION = roq::fix::Version::FIX_44;
auto const LOGOUT_RESPONSE = "LOGOUT"sv;
}  // namespace

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
    : sender_comp_id_{settings.fix.sender_comp_id}, target_comp_id_{settings.fix.target_comp_id},
      ping_freq_{settings.fix.ping_freq}, debug_{settings.fix.debug},
      connection_factory_{create_connection_factory(settings, context, uri)},
      connection_manager_{create_connection_manager(*this, settings, *connection_factory_)},
      decode_buffer_(settings.fix.decode_buffer_size), encode_buffer_(settings.fix.encode_buffer_size) {
}

void Session::operator()(roq::Event<roq::Start> const &) {
  (*connection_manager_).start();
}

void Session::operator()(roq::Event<roq::Stop> const &) {
  (*connection_manager_).stop();
}

void Session::operator()(roq::Event<roq::Timer> const &event) {
  auto now = event.value.now;
  (*connection_manager_).refresh(now);
  if (!ready())
    return;
  if (next_heartbeat_ <= now) {
    next_heartbeat_ = now + ping_freq_;
    send_test_request(now);
  }
}

bool Session::ready() const {
  return connection_status_ == roq::ConnectionStatus::READY;
}

void Session::operator()(roq::ConnectionStatus connection_status) {
  if (!roq::utils::update(connection_status_, connection_status))
    return;
  roq::log::debug("connection_status={}"sv, connection_status);
}

// io::net::ConnectionManager::Handler

void Session::operator()(roq::io::net::ConnectionManager::Connected const &) {
  roq::log::debug("Connected"sv);
  send_logon();
  (*this)(roq::ConnectionStatus::LOGIN_SENT);
}

void Session::operator()(roq::io::net::ConnectionManager::Disconnected const &) {
  roq::log::debug("Disconnected"sv);
  outbound_ = {};
  inbound_ = {};
  next_heartbeat_ = {};
  (*this)(roq::ConnectionStatus::DISCONNECTED);
}

void Session::operator()(roq::io::net::ConnectionManager::Read const &) {
  auto logger = [this](auto &message) {
    if (debug_) [[unlikely]]
      roq::log::info("{}"sv, roq::debug::fix::Message{message});
  };
  auto buffer = (*connection_manager_).buffer();
  size_t total_bytes = 0;
  while (!std::empty(buffer)) {
    roq::TraceInfo trace_info;
    auto parser = [&](auto &message) {
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
    };
    auto bytes = roq::fix::Reader<FIX_VERSION>::dispatch(buffer, parser, logger);
    if (bytes == 0)
      break;
    assert(bytes <= std::size(buffer));
    total_bytes += bytes;
    buffer = buffer.subspan(bytes);
  }
  (*connection_manager_).drain(total_bytes);
}

// inbound

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
  auto &header = message.header;
  switch (header.msg_type) {
    using enum roq::fix::MsgType;
    case HEARTBEAT: {
      auto heartbeat = roq::fix_bridge::fix::Heartbeat::create(message);
      dispatch(event, heartbeat);
      break;
    }
    case LOGON: {
      auto logon = roq::fix_bridge::fix::Logon::create(message);
      dispatch(event, logon);
      break;
    }
    case LOGOUT: {
      auto logout = roq::fix_bridge::fix::Heartbeat::create(message);
      dispatch(event, logout);
      break;
    }
    case RESEND_REQUEST: {
      auto resend_request = roq::fix_bridge::fix::ResendRequest::create(message);
      dispatch(event, resend_request);
      break;
    }
    case TEST_REQUEST: {
      auto test_request = roq::fix_bridge::fix::TestRequest::create(message);
      dispatch(event, test_request);
      break;
    }
    case EXECUTION_REPORT: {
      auto execution_report = roq::fix_bridge::fix::ExecutionReport::create(message, decode_buffer_);
      dispatch(event, execution_report);
      break;
    }
    case ORDER_CANCEL_REJECT: {
      auto order_cancel_reject = roq::fix_bridge::fix::OrderCancelReject::create(message, decode_buffer_);
      dispatch(event, order_cancel_reject);
      break;
    }
    case REJECT: {
      auto reject = roq::fix_bridge::fix::Reject::create(message);
      dispatch(event, reject);
      break;
    }
    default:
      roq::log::warn("Unexpected msg_type={}"sv, header.msg_type);
  }
}

template <typename T>
void Session::dispatch(roq::Trace<roq::fix::Message> const &event, T const &value) {
  auto &[trace_info, message] = event;
  roq::Trace event_2{trace_info, value};
  (*this)(event_2);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::Heartbeat> const &event) {
  auto &[trace_info, heartbeat] = event;
  roq::log::debug("heartbeat={}, trace_info={}"sv, heartbeat, trace_info);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::Logon> const &event) {
  auto &[trace_info, logon] = event;
  roq::log::debug("logon={}, trace_info={}"sv, logon, trace_info);
  // XXX TODO download + subscribe
  (*this)(roq::ConnectionStatus::READY);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::Logout> const &event) {
  auto &[trace_info, logout] = event;
  roq::log::debug("logout={}, trace_info={}"sv, logout, trace_info);
  // note! mandated, must send a logout response
  send_logout(LOGOUT_RESPONSE);
  roq::log::warn("closing connection"sv);
  (*connection_manager_).close();
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::ResendRequest> const &event) {
  auto &[trace_info, resend_request] = event;
  roq::log::debug("resend_request={}, trace_info={}"sv, resend_request, trace_info);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::TestRequest> const &event) {
  auto &[trace_info, test_request] = event;
  roq::log::debug("test_request={}, trace_info={}"sv, test_request, trace_info);
  send_heartbeat(test_request.test_req_id);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::ExecutionReport> const &event) {
  auto &[trace_info, execution_report] = event;
  roq::log::debug("execution_report={}, trace_info={}"sv, execution_report, trace_info);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::OrderCancelReject> const &event) {
  auto &[trace_info, order_cancel_reject] = event;
  roq::log::debug("order_cancel_reject={}, trace_info={}"sv, order_cancel_reject, trace_info);
}

void Session::operator()(roq::Trace<roq::fix_bridge::fix::Reject> const &event) {
  auto &[trace_info, reject] = event;
  roq::log::debug("reject={}, trace_info={}"sv, reject, trace_info);
}

// outbound

void Session::send_logon() {
  auto logon = roq::fix_bridge::fix::Logon{
      .encrypt_method = {},
      .heart_bt_int = std::chrono::duration_cast<std::chrono::seconds>(ping_freq_).count(),
      .reset_seq_num_flag = true,
      .next_expected_msg_seq_num = inbound_.msg_seq_num + 1,  // note!
      .username = {},
      .password = {},
  };
  send(logon);
}

void Session::send_logout(std::string_view const &text) {
  auto logout = roq::fix_bridge::fix::Logout{
      .text = text,
  };
  send(logout);
}

void Session::send_test_request(std::chrono::nanoseconds now) {
  auto test_req_id = fmt::format("{}"sv, now.count());
  auto test_request = roq::fix_bridge::fix::TestRequest{
      .test_req_id = test_req_id,
  };
  send(test_request);
}

void Session::send_heartbeat(std::string_view const &test_req_id) {
  auto heartbeat = roq::fix_bridge::fix::Heartbeat{
      .test_req_id = test_req_id,
  };
  send(heartbeat);
}

template <typename T>
void Session::send(T const &event) {
  auto sending_time = roq::clock::get_realtime();
  auto header = roq::fix::Header{
      .version = FIX_VERSION,
      .msg_type = T::msg_type,
      .sender_comp_id = sender_comp_id_,
      .target_comp_id = target_comp_id_,
      .msg_seq_num = ++outbound_.msg_seq_num,  // note!
      .sending_time = sending_time,
  };
  auto message = event.encode(header, encode_buffer_);
  if (debug_) [[unlikely]]
    roq::log::info("{}"sv, roq::debug::fix::Message{message});
  (*connection_manager_).send(message);
}

}  // namespace fix
}  // namespace simple
