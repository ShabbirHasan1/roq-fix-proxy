/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/client/fix/session.hpp"

#include "roq/logging.hpp"

#include "roq/utils/chrono.hpp"  // hh_mm_ss

#include "roq/oms/exceptions.hpp"

using namespace std::literals;

namespace roq {
namespace proxy {
namespace fix {
namespace client {
namespace fix {

// === CONSTANTS ===

namespace {
auto const FIX_VERSION = roq::fix::Version::FIX_44;

auto const ERROR_GOODBYE = "goodbye"sv;
auto const ERROR_MISSING_HEARTBEAT = "MISSING HEARTBEAT"sv;
auto const ERROR_NO_LOGON = "NO LOGON"sv;
auto const ERROR_UNEXPECTED_LOGON = "UNEXPECTED LOGON"sv;
auto const ERROR_UNEXPECTED_MSG_TYPE = "UNEXPECTED MSG_TYPE"sv;
auto const ERROR_UNKNOWN_TARGET_COMP_ID = "UNKNOWN TARGET_COMP_ID"sv;
auto const ERROR_UNSUPPORTED_MSG_TYPE = "UNSUPPORTED MSG_TYPE"sv;
auto const ERROR_UNSUPPORTED_PARTY_IDS = "UNSUPPORTED PARTY_IDS"sv;
auto const ERROR_CREATE_ROUTE_TIMEOUT = "CREATE_ROUTE_TIMEOUT"sv;
}  // namespace

// === HELPERS ===

namespace {
auto create_logon_timeout(auto &settings) {
  auto now = clock::get_system();
  return now + settings.client.logon_timeout;
}
}  // namespace

// === IMPLEMENTATION ===

Session::Session(
    client::Session::Handler &handler, uint64_t session_id, io::net::tcp::Connection::Factory &factory, Shared &shared)
    : handler_{handler}, session_id_{session_id}, connection_{factory.create(*this)}, shared_{shared},
      logon_timeout_{create_logon_timeout(shared_.settings)}, decode_buffer_(shared.settings.client.decode_buffer_size),
      encode_buffer_(shared.settings.client.encode_buffer_size) {
}

void Session::operator()(Event<Stop> const &) {
}

void Session::operator()(Event<Timer> const &event) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
      if (logon_timeout_ < event.value.now) {
        log::warn("Closing connection (reason: client did not send a logon message)"sv);
        close();
      }
      break;
    case WAITING_USER_RESPONSE: {
      assert(user_response_timeout_.count());
      if (user_response_timeout_ < event.value.now) {
        auto logout = codec::fix::Logout{
            .text = ERROR_CREATE_ROUTE_TIMEOUT,
        };
        send_and_close<2>(logout);
      }
      break;
    }
    case READY:
      if (next_heartbeat_ < event.value.now) {
        next_heartbeat_ = event.value.now + shared_.settings.client.heartbeat_freq;
        if (waiting_for_heartbeat_) {
          log::warn("Closing connection (reason: client did not send heartbeat)"sv);
          auto logout = codec::fix::Logout{
              .text = ERROR_MISSING_HEARTBEAT,
          };
          send_and_close<2>(logout);
        } else {
          auto test_req_id = fmt::format("{}"sv, event.value.now);  // XXX TODO something else
          auto test_request = codec::fix::TestRequest{
              .test_req_id = test_req_id,
          };
          send<4>(test_request);
          waiting_for_heartbeat_ = true;
        }
      }
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::BusinessMessageReject> const &event) {
  auto &[trace_info, business_message_reject] = event;
  if (ready())
    send<2>(business_message_reject);
}

void Session::operator()(Trace<codec::fix::UserResponse> const &event) {
  auto &[trace_info, user_response] = event;
  user_response_timeout_ = {};
  if (user_response.user_status == roq::fix::UserStatus::LOGGED_IN) {
    if (state_ == State::WAITING_USER_RESPONSE) {
      auto heart_bt_int = std::chrono::duration_cast<std::chrono::seconds>(shared_.settings.client.heartbeat_freq);
      auto response = codec::fix::Logon{
          .encrypt_method = roq::fix::EncryptMethod::NONE,
          .heart_bt_int = static_cast<uint16_t>(heart_bt_int.count()),
          .reset_seq_num_flag = {},
          .next_expected_msg_seq_num = {},
          .username = {},
          .password = {},
      };
      log::debug("logon={}"sv, response);
      send<2>(response);
      state_ = State::READY;
    } else {
      log::fatal("Unexpected"sv);
    }
  } else {
    make_zombie();
  }
}

void Session::operator()(Trace<codec::fix::SecurityList> const &event) {
  auto &[trace_info, security_list] = event;
  if (ready())
    send<2>(security_list);
}

void Session::operator()(Trace<codec::fix::SecurityDefinition> const &event) {
  auto &[trace_info, security_definition] = event;
  if (ready())
    send<2>(security_definition);
}

void Session::operator()(Trace<codec::fix::SecurityStatus> const &event) {
  auto &[trace_info, security_status] = event;
  if (ready())
    send<2>(security_status);
}

void Session::operator()(Trace<codec::fix::MarketDataRequestReject> const &event) {
  auto &[trace_info, market_data_request_reject] = event;
  if (ready())
    send<2>(market_data_request_reject);
}

void Session::operator()(Trace<codec::fix::MarketDataSnapshotFullRefresh> const &event) {
  auto &[trace_info, market_data_snapshot_full_refresh] = event;
  if (ready())
    send<2>(market_data_snapshot_full_refresh);
}

void Session::operator()(Trace<codec::fix::MarketDataIncrementalRefresh> const &event) {
  auto &[trace_info, market_data_incremental_refresh] = event;
  if (ready())
    send<2>(market_data_incremental_refresh);
}

void Session::operator()(Trace<codec::fix::OrderCancelReject> const &event) {
  auto &[trace_info, order_cancel_reject] = event;
  if (ready())
    send<2>(order_cancel_reject);
}

void Session::operator()(Trace<codec::fix::ExecutionReport> const &event) {
  auto &[trace_info, execution_report] = event;
  if (ready())
    send<2>(execution_report);
}

bool Session::ready() const {
  return state_ == State::READY;
}

bool Session::zombie() const {
  return state_ == State::ZOMBIE;
}

void Session::force_disconnect() {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
    case READY:
      close();
      break;
    case ZOMBIE:
      break;
  }
}

void Session::close() {
  if (state_ != State::ZOMBIE) {
    (*connection_).close();
    make_zombie();
  }
}

// io::net::tcp::Connection::Handler

void Session::operator()(io::net::tcp::Connection::Read const &) {
  if (state_ == State::ZOMBIE)
    return;
  buffer_.append(*connection_);
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
  make_zombie();
}

// utilities

void Session::make_zombie() {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
      break;
    case WAITING_USER_RESPONSE:
    case READY: {
      TraceInfo trace_info;
      auto disconnect = Session::Disconnected{
          .session_id = session_id_,
      };
      Trace event{trace_info, disconnect};
      handler_(event, username_);
      break;
    }
    case ZOMBIE:
      return;
  }
  state_ = State::ZOMBIE;
  shared_.session_remove(session_id_);
}

template <std::size_t level, typename T>
void Session::send_and_close(T const &event) {
  assert(state_ != State::ZOMBIE);
  auto sending_time = clock::get_realtime();
  send<level>(event, sending_time);
  close();
}

template <std::size_t level, typename T>
void Session::send(T const &event) {
#ifndef NDEBUG
  auto can_send = [&]() {
    if constexpr (std::is_same<T, codec::fix::Logon>::value) {
      return state_ == State::WAITING_USER_RESPONSE;
    }
    return state_ == State::READY;
  };
  assert(can_send());
#endif
  auto sending_time = clock::get_realtime();
  send<level>(event, sending_time);
}

template <std::size_t level, typename T>
void Session::send(T const &event, std::chrono::nanoseconds sending_time) {
  log::info<level>("sending: event={}"sv, event);
  assert(!std::empty(comp_id_));
  auto header = roq::fix::Header{
      .version = FIX_VERSION,
      .msg_type = T::MSG_TYPE,
      .sender_comp_id = shared_.settings.client.comp_id,
      .target_comp_id = comp_id_,
      .msg_seq_num = ++outbound_.msg_seq_num,  // note!
      .sending_time = sending_time,
  };
  auto message = event.encode(header, encode_buffer_);
  (*connection_).send(message);
}

void Session::check(roq::fix::Header const &header) {
  auto current = header.msg_seq_num;
  auto expected = inbound_.msg_seq_num + 1;
  if (current != expected) [[unlikely]] {
    if (expected < current) {
      log::warn(
          "*** SEQUENCE GAP *** "
          "current={} previous={} distance={}"sv,
          current,
          inbound_.msg_seq_num,
          current - inbound_.msg_seq_num);
    } else {
      log::warn(
          "*** SEQUENCE REPLAY *** "
          "current={} previous={} distance={}"sv,
          current,
          inbound_.msg_seq_num,
          inbound_.msg_seq_num - current);
    }
  }
  inbound_.msg_seq_num = current;
}

void Session::parse(Trace<roq::fix::Message> const &event) {
  auto &[trace_info, message] = event;
  switch (message.header.msg_type) {
    using enum roq::fix::MsgType;
    // - session
    case TEST_REQUEST:
      dispatch<codec::fix::TestRequest>(event);
      break;
    case RESEND_REQUEST:
      dispatch<codec::fix::ResendRequest>(event);
      break;
    case REJECT:
      dispatch<codec::fix::Reject>(event);
      break;
    case HEARTBEAT:
      dispatch<codec::fix::Heartbeat>(event);
      break;
      // - authentication
    case LOGON:
      dispatch<codec::fix::Logon>(event);
      break;
    case LOGOUT:
      dispatch<codec::fix::Logout>(event);
      break;
      // - market data
    case TRADING_SESSION_STATUS_REQUEST:
      dispatch<codec::fix::TradingSessionStatusRequest>(event);
      break;
    case SECURITY_LIST_REQUEST:
      dispatch<codec::fix::SecurityListRequest>(event);
      break;
    case SECURITY_DEFINITION_REQUEST:
      dispatch<codec::fix::SecurityDefinitionRequest>(event, decode_buffer_);
      break;
    case SECURITY_STATUS_REQUEST:
      dispatch<codec::fix::SecurityStatusRequest>(event, decode_buffer_);
      break;
    case MARKET_DATA_REQUEST:
      dispatch<codec::fix::MarketDataRequest>(event, decode_buffer_);
      break;
      // - order management
    case ORDER_STATUS_REQUEST:
      dispatch<codec::fix::OrderStatusRequest>(event, decode_buffer_);
      break;
    case ORDER_MASS_STATUS_REQUEST:
      dispatch<codec::fix::OrderMassStatusRequest>(event, decode_buffer_);
      break;
    case NEW_ORDER_SINGLE:
      dispatch<codec::fix::NewOrderSingle>(event, decode_buffer_);
      break;
    case ORDER_CANCEL_REQUEST:
      dispatch<codec::fix::OrderCancelRequest>(event, decode_buffer_);
      break;
    case ORDER_CANCEL_REPLACE_REQUEST:
      dispatch<codec::fix::OrderCancelReplaceRequest>(event, decode_buffer_);
      break;
    case ORDER_MASS_CANCEL_REQUEST:
      dispatch<codec::fix::OrderMassCancelRequest>(event, decode_buffer_);
      break;
      // - trade capture reporting
      dispatch<codec::fix::TradeCaptureReportRequest>(event, decode_buffer_);
      break;
    case REQUEST_FOR_POSITIONS:
      dispatch<codec::fix::RequestForPositions>(event, decode_buffer_);
      break;
    default:
      log::warn("Unexpected: msg_type={}"sv, message.header.msg_type);
      send_business_message_reject(
          message.header, roq::fix::BusinessRejectReason::UNSUPPORTED_MESSAGE_TYPE, ERROR_UNEXPECTED_MSG_TYPE);
      break;
  };
}

template <typename T, typename... Args>
void Session::dispatch(Trace<roq::fix::Message> const &event, Args &&...args) {
  auto &[trace_info, message] = event;
  auto value = T::create(message, std::forward<Args>(args)...);
  Trace event_2{trace_info, value};
  (*this)(event_2, message.header);
}

void Session::operator()(Trace<codec::fix::TestRequest> const &event, roq::fix::Header const &header) {
  auto &[trace_info, test_request] = event;
  log::info<1>("test_request={}"sv, test_request);
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case WAITING_USER_RESPONSE:
    case READY: {
      auto heartbeat = codec::fix::Heartbeat{
          .test_req_id = test_request.test_req_id,
      };
      send<4>(heartbeat);
      break;
    }
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::ResendRequest> const &event, roq::fix::Header const &header) {
  auto &[trace_info, resend_request] = event;
  log::info<1>("resend_request={}"sv, resend_request);
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      send_business_message_reject(
          header, roq::fix::BusinessRejectReason::UNSUPPORTED_MESSAGE_TYPE, ERROR_UNSUPPORTED_MSG_TYPE);
      break;
    case ZOMBIE:
      assert(false);
      break;
  }
}

void Session::operator()(Trace<codec::fix::Reject> const &event, roq::fix::Header const &) {
  auto &[trace_info, reject] = event;
  log::warn("reject={}"sv, reject);
  close();
}

void Session::operator()(Trace<codec::fix::Heartbeat> const &event, roq::fix::Header const &header) {
  auto &[trace_info, heartbeat] = event;
  log::info<1>("heartbeat={}"sv, heartbeat);
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      waiting_for_heartbeat_ = false;
      break;
    case ZOMBIE:
      break;
  }
}

// business

// session

void Session::operator()(Trace<codec::fix::Logon> const &event, roq::fix::Header const &header) {
  auto &trace_info = event.trace_info;
  auto &logon = event.value;
  switch (state_) {
    using enum State;
    case WAITING_LOGON: {
      comp_id_ = header.sender_comp_id;
      if (header.target_comp_id != shared_.settings.client.comp_id) {
        log::error(
            R"(Unexpected target_comp_id="{}" (expected: "{}"))"sv,
            header.target_comp_id,
            shared_.settings.client.comp_id);
        send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_UNKNOWN_TARGET_COMP_ID);
      } else {
        auto success = [&](auto strategy_id) {
          state_ = State::READY;
          username_ = logon.username;
          party_id_ = fmt::format("{}"sv, strategy_id);
          try {
            auto user_request_id = shared_.create_request_id();
            auto user_request = codec::fix::UserRequest{
                .user_request_id = user_request_id,
                .user_request_type = roq::fix::UserRequestType::LOG_ON_USER,
                .username = party_id_,
                .password = {},
                .new_password = {},
            };
            Trace event_2{trace_info, user_request};
            handler_(event_2, username_, session_id_);
            state_ = State::WAITING_USER_RESPONSE;
            auto now = clock::get_system();
            user_response_timeout_ = now + shared_.settings.server.request_timeout;
          } catch (oms::Exception &e) {
            send_reject(header, roq::fix::SessionRejectReason::OTHER, e.what());
          }
        };
        auto failure = [&](auto &reason) {
          log::error("Invalid logon (reason: {})"sv, reason);
          send_reject(header, roq::fix::SessionRejectReason::OTHER, reason);
        };
        shared_.session_logon(session_id_, logon.username, logon.password, success, failure);
      }
      break;
    }
    case WAITING_USER_RESPONSE:
    case READY:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_UNEXPECTED_LOGON);
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::Logout> const &event, roq::fix::Header const &header) {
  auto &[trace_info, logout] = event;
  log::info<1>("logout={}"sv, logout);
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY: {
      assert(!std::empty(party_id_));
      auto user_request_id = shared_.create_request_id();
      auto user_request = codec::fix::UserRequest{
          .user_request_id = user_request_id,
          .user_request_type = roq::fix::UserRequestType::LOG_OFF_USER,
          .username = party_id_,
          .password = {},
          .new_password = {},
      };
      Trace event_2{trace_info, user_request};
      handler_(event_2, username_, session_id_);
      auto success = [&]() {
        username_.clear();
        auto response = codec::fix::Logout{
            .text = ERROR_GOODBYE,
        };
        send_and_close<2>(response);
      };
      auto failure = [&](auto &reason) { send_reject(header, roq::fix::SessionRejectReason::OTHER, reason); };
      shared_.session_logout(session_id_, success, failure);
      break;
    }
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::TradingSessionStatusRequest> const &, roq::fix::Header const &header) {
  send_business_message_reject(
      header,
      roq::fix::BusinessRejectReason::UNSUPPORTED_MESSAGE_TYPE,
      ERROR_UNEXPECTED_MSG_TYPE);  // XXX TODO
}

void Session::operator()(Trace<codec::fix::SecurityListRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      handler_(event, username_);
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::SecurityDefinitionRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      handler_(event, username_);
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::SecurityStatusRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      handler_(event, username_);
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::MarketDataRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      handler_(event, username_, session_id_);
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::OrderStatusRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      handler_(event, username_);
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::OrderMassStatusRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      handler_(event, username_);
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::NewOrderSingle> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      if (add_party_ids(event, [&](auto &event_2) { handler_(event_2, username_); })) {
      } else {
        send_business_message_reject(header, roq::fix::BusinessRejectReason::OTHER, ERROR_UNSUPPORTED_PARTY_IDS);
      }
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::OrderCancelRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      if (add_party_ids(event, [&](auto &event_2) { handler_(event_2, username_); })) {
      } else {
        send_business_message_reject(header, roq::fix::BusinessRejectReason::OTHER, ERROR_UNSUPPORTED_PARTY_IDS);
      }
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::OrderCancelReplaceRequest> const &event, roq::fix::Header const &header) {
  switch (state_) {
    using enum State;
    case WAITING_LOGON:
    case WAITING_USER_RESPONSE:
      send_reject(header, roq::fix::SessionRejectReason::OTHER, ERROR_NO_LOGON);
      break;
    case READY:
      if (add_party_ids(event, [&](auto &event_2) { handler_(event_2, username_); })) {
      } else {
        send_business_message_reject(header, roq::fix::BusinessRejectReason::OTHER, ERROR_UNSUPPORTED_PARTY_IDS);
      }
      break;
    case ZOMBIE:
      break;
  }
}

void Session::operator()(Trace<codec::fix::OrderMassCancelRequest> const &, roq::fix::Header const &header) {
  send_business_message_reject(
      header,
      roq::fix::BusinessRejectReason::UNSUPPORTED_MESSAGE_TYPE,
      ERROR_UNEXPECTED_MSG_TYPE);  // XXX TODO
}

void Session::operator()(Trace<codec::fix::TradeCaptureReportRequest> const &, roq::fix::Header const &header) {
  send_business_message_reject(
      header,
      roq::fix::BusinessRejectReason::UNSUPPORTED_MESSAGE_TYPE,
      ERROR_UNEXPECTED_MSG_TYPE);  // XXX TODO
}

void Session::operator()(Trace<codec::fix::RequestForPositions> const &, roq::fix::Header const &header) {
  send_business_message_reject(
      header,
      roq::fix::BusinessRejectReason::UNSUPPORTED_MESSAGE_TYPE,
      ERROR_UNEXPECTED_MSG_TYPE);  // XXX TODO
}

void Session::send_reject(
    roq::fix::Header const &header, roq::fix::SessionRejectReason session_reject_reason, std::string_view const &text) {
  auto response = codec::fix::Reject{
      .ref_seq_num = header.msg_seq_num,
      .text = text,
      .ref_tag_id = {},
      .ref_msg_type = header.msg_type,
      .session_reject_reason = session_reject_reason,
  };
  log::warn("reject={}"sv, response);
  send_and_close<2>(response);
}

void Session::send_business_message_reject(
    roq::fix::Header const &header,
    roq::fix::BusinessRejectReason business_reject_reason,
    std::string_view const &text) {
  auto response = codec::fix::BusinessMessageReject{
      .ref_seq_num = header.msg_seq_num,
      .ref_msg_type = header.msg_type,
      .business_reject_ref_id = {},
      .business_reject_reason = business_reject_reason,
      .text = text,
  };
  log::warn("business_message_reject={}"sv, response);
  send<2>(response);
}

template <typename T, typename Callback>
bool Session::add_party_ids(Trace<T> const &event, Callback callback) const {
  assert(!std::empty(party_id_));
  auto &[trace_info, value] = event;
  if (std::empty(value.no_party_ids)) {
    auto party = codec::fix::Party{
        .party_id = party_id_,
        .party_id_source = roq::fix::PartyIDSource::PROPRIETARY_CUSTOM_CODE,
        .party_role = roq::fix::PartyRole::CLIENT_ID,
    };
    auto value_2 = value;
    value_2.no_party_ids = {&party, 1};
    Trace event_2{trace_info, value_2};
    callback(event_2);
    return true;
  }
  return false;
}

}  // namespace fix
}  // namespace client
}  // namespace fix
}  // namespace proxy
}  // namespace roq
