/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/fix_proxy/client/fix/session.hpp"

#include "roq/logging.hpp"

#include "roq/fix_bridge/fix/logon.hpp"
#include "roq/fix_bridge/fix/logout.hpp"

using namespace std::literals;

namespace roq {
namespace fix_proxy {
namespace client {
namespace fix {

// === CONSTANTS ===

namespace {
auto const FIX_VERSION = roq::fix::Version::FIX_44;
auto const ERROR_UNEXPECTED_MSG_TYPE = "UNEXPECTED MSG_TYPE"sv;
}  // namespace

// === IMPLEMENTATION ===

Session::Session(
    client::Session::Handler &handler, uint64_t session_id, io::net::tcp::Connection::Factory &factory, Shared &shared)
    : handler_{handler}, session_id_{session_id}, connection_{factory.create(*this)}, shared_{shared},
      decode_buffer_(shared.settings.client.decode_buffer_size),
      encode_buffer_(shared.settings.client.encode_buffer_size) {
}

void Session::operator()(Event<Stop> const &) {
}

void Session::operator()(Event<Timer> const &) {
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
  if (state_ == State::ZOMBIE)
    return;
  auto buffer = buffer_.data();
  try {
    size_t total_bytes = 0;
    auto parser = [&](auto &message) {
      TraceInfo trace_info;
      check(message.header);
      Trace event{trace_info, message};
      parse(event);
    };
    auto logger = [&]([[maybe_unused]] auto &message) {
      // note! here we could log the raw binary message
    };
    while (!std::empty(buffer)) {
      auto bytes = roq::fix::Reader<FIX_VERSION>::dispatch(buffer, parser, logger);
      if (bytes == 0)
        break;
      assert(bytes <= std::size(buffer));
      total_bytes += bytes;
      buffer = buffer.subspan(bytes);
      if (state_ == State::ZOMBIE)
        break;
    }
    buffer_.drain(total_bytes);
  } catch (Exception &e) {
    log::error("Exception: {}"sv, e);
    close();
  } catch (std::exception &e) {
    log::error("Exception: {}"sv, e.what());
    close();
  } catch (...) {
    auto e = std::current_exception();
    log::fatal(R"(Unhandled exception: type="{}")"sv, typeid(e).name());
  }
}

void Session::operator()(io::net::tcp::Connection::Disconnected const &) {
}

// utilities

template <std::size_t level, typename T>
void Session::send(T const &event) {
  assert(state_ == State::READY);
  auto sending_time = clock::get_realtime();
  send_helper<level>(event, sending_time);
}

template <std::size_t level, typename T>
void Session::send(T const &event, std::chrono::nanoseconds sending_time, std::chrono::nanoseconds origin_create_time) {
  /*
  log::info<level>("{} sending: event={}"sv, prefix_, event);
  assert(!std::empty(comp_id_));
  auto header = roq::fix::Header{
      .version = FIX_VERSION,
      .msg_type = T::MSG_TYPE,
      .sender_comp_id = shared_.settings.fix.fix_comp_id,
      .target_comp_id = comp_id_,
      .msg_seq_num = ++outbound_.msg_seq_num,  // note!
      .sending_time = sending_time,
  };
  auto message = event.encode(header, encode_buffer_);
  shared_.fix_log.sending<level>(id_, message);
  (*connection_).send(message);
  if (origin_create_time.count() == 0)
    return;
  auto now = clock::get_system();
  auto latency = now - origin_create_time;
  shared_.latency.end_to_end.update(latency.count());
  */
}

void Session::check(roq::fix::Header const &header) {
  /*
  auto current = header.msg_seq_num;
  auto expected = inbound_.msg_seq_num + 1;
  if (current != expected) [[unlikely]] {
    if (expected < current) {
      log::warn(
          "{} "
          "*** SEQUENCE GAP *** "
          "current={} previous={} distance={}"sv,
          prefix_,
          current,
          inbound_.msg_seq_num,
          current - inbound_.msg_seq_num);
    } else {
      log::warn(
          "{} "
          "*** SEQUENCE REPLAY *** "
          "current={} previous={} distance={}"sv,
          prefix_,
          current,
          inbound_.msg_seq_num,
          inbound_.msg_seq_num - current);
    }
  }
  inbound_.msg_seq_num = current;
  */
}

void Session::parse(Trace<roq::fix::Message> const &event) {
  auto &[trace_info, message] = event;
  switch (message.header.msg_type) {
    using enum roq::fix::MsgType;
    // session
    case LOGON:
      dispatch<fix_bridge::fix::Logon>(event);
      break;
    case LOGOUT:
      dispatch<fix_bridge::fix::Logout>(event);
      break;
    case TEST_REQUEST:
      dispatch<fix_bridge::fix::TestRequest>(event);
      break;
    case RESEND_REQUEST:
      dispatch<fix_bridge::fix::ResendRequest>(event);
      break;
    case REJECT:
      dispatch<fix_bridge::fix::Reject>(event);
      break;
    case HEARTBEAT:
      dispatch<fix_bridge::fix::Heartbeat>(event);
      break;
      // business
      // - trading session
    case TRADING_SESSION_STATUS_REQUEST:
      dispatch<fix_bridge::fix::TradingSessionStatusRequest>(event);
      break;
      // - market data
    case SECURITY_LIST_REQUEST:
      dispatch<fix_bridge::fix::SecurityListRequest>(event);
      break;
    case SECURITY_DEFINITION_REQUEST:
      dispatch<fix_bridge::fix::SecurityDefinitionRequest>(event, decode_buffer_);
      break;
    case SECURITY_STATUS_REQUEST:
      dispatch<fix_bridge::fix::SecurityStatusRequest>(event, decode_buffer_);
      break;
    case MARKET_DATA_REQUEST:
      dispatch<fix_bridge::fix::MarketDataRequest>(event, decode_buffer_);
      break;
      // - order management
    case ORDER_STATUS_REQUEST:
      dispatch<fix_bridge::fix::OrderStatusRequest>(event, decode_buffer_);
      break;
    case ORDER_MASS_STATUS_REQUEST:
      dispatch<fix_bridge::fix::OrderMassStatusRequest>(event, decode_buffer_);
      break;
    case NEW_ORDER_SINGLE:
      dispatch<fix_bridge::fix::NewOrderSingle>(event, decode_buffer_);
      break;
    case ORDER_CANCEL_REQUEST:
      dispatch<fix_bridge::fix::OrderCancelRequest>(event, decode_buffer_);
      break;
    case ORDER_CANCEL_REPLACE_REQUEST:
      dispatch<fix_bridge::fix::OrderCancelReplaceRequest>(event, decode_buffer_);
      break;
    case ORDER_MASS_CANCEL_REQUEST:
      dispatch<fix_bridge::fix::OrderMassCancelRequest>(event, decode_buffer_);
      break;
      // - trade capture reporting
      dispatch<fix_bridge::fix::TradeCaptureReportRequest>(event, decode_buffer_);
      break;
    case REQUEST_FOR_POSITIONS:
      dispatch<fix_bridge::fix::RequestForPositions>(event, decode_buffer_);
      break;
    default: {
      log::warn("Unexpected: msg_type={}"sv, message.header.msg_type);
      auto response = fix_bridge::fix::BusinessMessageReject{
          .ref_seq_num = message.header.msg_seq_num,
          .ref_msg_type = message.header.msg_type,
          .business_reject_ref_id = {},
          .business_reject_reason = roq::fix::BusinessRejectReason::UNSUPPORTED_MESSAGE_TYPE,
          .text = ERROR_UNEXPECTED_MSG_TYPE,
      };
      // send<2>(response);
      break;
    }
  };
}

template <typename T, typename... Args>
void Session::dispatch(Trace<roq::fix::Message> const &event, Args &&...args) {
  auto &[trace_info, message] = event;
  auto value = T::create(message, std::forward<Args>(args)...);
  Trace event_2{trace_info, value};
  (*this)(event_2, message.header);
}

// session

void Session::operator()(Trace<fix_bridge::fix::Logon> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::Logout> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::TestRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::ResendRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::Reject> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::Heartbeat> const &, roq::fix::Header const &) {
  // XXX
}

// business

void Session::operator()(Trace<fix_bridge::fix::TradingSessionStatusRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::SecurityListRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::SecurityDefinitionRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::SecurityStatusRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::MarketDataRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::OrderStatusRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::OrderMassStatusRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::NewOrderSingle> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::OrderCancelRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::OrderCancelReplaceRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::OrderMassCancelRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::TradeCaptureReportRequest> const &, roq::fix::Header const &) {
  // XXX
}

void Session::operator()(Trace<fix_bridge::fix::RequestForPositions> const &, roq::fix::Header const &) {
  // XXX
}

}  // namespace fix
}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
