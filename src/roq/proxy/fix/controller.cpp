/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/controller.hpp"

#include "roq/event.hpp"
#include "roq/timer.hpp"

#include "roq/oms/exceptions.hpp"

#include "roq/logging.hpp"

using namespace std::literals;

namespace roq {
namespace proxy {
namespace fix {

// === CONSTANTS ===

namespace {
auto const TIMER_FREQUENCY = 100ms;
}

// === HELPERS ===

namespace {
auto create_server_session(auto &handler, auto &settings, auto &context, auto &shared, auto &connections) {
  if (std::size(connections) != 1)
    log::fatal("Unexpected: only supporting a single upstream fix-bridge"sv);
  auto &connection = connections[0];
  auto uri = io::web::URI{connection};
  log::debug("{}"sv, uri);
  return server::Session(handler, settings, context, shared, uri);
}

template <typename T>
auto get_client_from_parties(T &value) -> std::string_view {
  using value_type = std::remove_cvref<T>::type;
  auto const &party_ids = [&]() {
    if constexpr (std::is_same<value_type, roq::codec::fix::TradeCaptureReport>::value) {
      // assert(!std::empty(value.no_sides));
      return value.no_sides[0].no_party_ids;
    } else {
      return value.no_party_ids;
    }
  }();
  if (std::empty(party_ids))
    return {};
  if (std::size(party_ids) == 1)
    for (auto &item : party_ids) {
      if (!std::empty(item.party_id) && item.party_id_source == roq::fix::PartyIDSource::PROPRIETARY_CUSTOM_CODE &&
          item.party_role == roq::fix::PartyRole::CLIENT_ID)
        return item.party_id;
    }
  log::warn("Unexpected: party_ids=[{}]"sv, fmt::join(party_ids, ", "sv));
  return {};
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
      server_session_{create_server_session(*this, settings, context, shared_, connections)},
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
}

// server::Session::Handler

void Controller::operator()(Trace<server::Session::Ready> const &) {
  ready_ = true;
}

void Controller::operator()(Trace<server::Session::Disconnected> const &) {
  ready_ = false;
  client_manager_.get_all_sessions([&](auto &session) { session.force_disconnect(); });
}

void Controller::operator()(Trace<codec::fix::BusinessMessageReject> const &event) {
  auto dispatch = [&](auto &mapping) {
    auto iter = mapping.server_to_client.find(event.value.business_reject_ref_id);
    if (iter != std::end(mapping.server_to_client)) {
      auto &[session_id, req_id, keep_alive] = (*iter).second;
      auto business_message_reject = event.value;
      // XXX FIXME what about ref_seq_num ???
      business_message_reject.business_reject_ref_id = req_id;
      Trace event_2{event.trace_info, business_message_reject};
      dispatch_to_client(event_2, session_id);
      // XXX FIXME what about keep_alive ???
    }
  };
  switch (event.value.ref_msg_type) {
    using enum roq::fix::MsgType;
    case UNDEFINED:
      break;
    case UNKNOWN:
      break;
    case HEARTBEAT:
      break;
    case TEST_REQUEST:
      break;
    case RESEND_REQUEST:
      break;
    case REJECT:
      break;
    case SEQUENCE_RESET:
      break;
    case LOGOUT:
      break;
    case IOI:
      break;
    case ADVERTISEMENT:
      break;
    case EXECUTION_REPORT:
      break;
    case ORDER_CANCEL_REJECT:
      break;
    case LOGON:
      break;
    case DERIVATIVE_SECURITY_LIST:
      break;
    case NEW_ORDER_MULTILEG:
      break;
    case MULTILEG_ORDER_CANCEL_REPLACE:
      break;
    case TRADE_CAPTURE_REPORT_REQUEST:
      dispatch(subscriptions_.trade_request_id);
      return;  // note!
    case TRADE_CAPTURE_REPORT:
      break;
    case ORDER_MASS_STATUS_REQUEST:
      dispatch(subscriptions_.mass_status_req_id);
      return;  // note!
    case QUOTE_REQUEST_REJECT:
      break;
    case RFQ_REQUEST:
      break;
    case QUOTE_STATUS_REPORT:
      break;
    case QUOTE_RESPONSE:
      break;
    case CONFIRMATION:
      break;
    case POSITION_MAINTENANCE_REQUEST:
      break;
    case POSITION_MAINTENANCE_REPORT:
      break;
    case REQUEST_FOR_POSITIONS:
      dispatch(subscriptions_.pos_req_id);
      return;  // note!
    case REQUEST_FOR_POSITIONS_ACK:
      break;
    case POSITION_REPORT:
      break;
    case TRADE_CAPTURE_REPORT_REQUEST_ACK:
      break;
    case TRADE_CAPTURE_REPORT_ACK:
      break;
    case ALLOCATION_REPORT:
      break;
    case ALLOCATION_REPORT_ACK:
      break;
    case CONFIRMATION_ACK:
      break;
    case SETTLEMENT_INSTRUCTION_REQUEST:
      break;
    case ASSIGNMENT_REPORT:
      break;
    case COLLATERAL_REQUEST:
      break;
    case COLLATERAL_ASSIGNMENT:
      break;
    case COLLATERAL_RESPONSE:
      break;
    case NEWS:
      break;
    case COLLATERAL_REPORT:
      break;
    case COLLATERAL_INQUIRY:
      break;
    case NETWORK_COUNTERPARTY_SYSTEM_STATUS_REQUEST:
      break;
    case NETWORK_COUNTERPARTY_SYSTEM_STATUS_RESPONSE:
      break;
    case USER_REQUEST:
      break;
    case USER_RESPONSE:
      break;
    case COLLATERAL_INQUIRY_ACK:
      break;
    case CONFIRMATION_REQUEST:
      break;
    case EMAIL:
      break;
    case NEW_ORDER_SINGLE:
      dispatch(subscriptions_.md_req_id);  // XXX HANS
      return;                              // note!
    case NEW_ORDER_LIST:
      break;
    case ORDER_CANCEL_REQUEST:
      dispatch(subscriptions_.md_req_id);  // XXX HANS
      return;                              // note!
    case ORDER_CANCEL_REPLACE_REQUEST:
      dispatch(subscriptions_.md_req_id);  // XXX HANS
      return;                              // note!
    case ORDER_STATUS_REQUEST:
      dispatch(subscriptions_.ord_status_req_id);
      return;  // note!
    case ALLOCATION_INSTRUCTION:
      break;
    case LIST_CANCEL_REQUEST:
      break;
    case LIST_EXECUTE:
      break;
    case LIST_STATUS_REQUEST:
      break;
    case LIST_STATUS:
      break;
    case ALLOCATION_INSTRUCTION_ACK:
      break;
    case DONT_KNOW_TRADE_DK:
      break;
    case QUOTE_REQUEST:
      break;
    case QUOTE:
      break;
    case SETTLEMENT_INSTRUCTIONS:
      break;
    case MARKET_DATA_REQUEST:
      dispatch(subscriptions_.md_req_id);
      return;  // note!
    case MARKET_DATA_SNAPSHOT_FULL_REFRESH:
      dispatch(subscriptions_.md_req_id);
      return;  // note!
    case MARKET_DATA_INCREMENTAL_REFRESH:
      dispatch(subscriptions_.md_req_id);
      return;  // note!
    case MARKET_DATA_REQUEST_REJECT:
      dispatch(subscriptions_.md_req_id);
      return;  // note!
    case QUOTE_CANCEL:
      break;
    case QUOTE_STATUS_REQUEST:
      break;
    case MASS_QUOTE_ACKNOWLEDGEMENT:
      break;
    case SECURITY_DEFINITION_REQUEST:
      dispatch(subscriptions_.security_req_id);
      return;  // note!
    case SECURITY_DEFINITION:
      break;
    case SECURITY_STATUS_REQUEST:
      dispatch(subscriptions_.security_status_req_id);
      return;  // note!
    case SECURITY_STATUS:
      break;
    case TRADING_SESSION_STATUS_REQUEST:
      dispatch(subscriptions_.md_req_id);  // XXX HANS
      return;                              // note!
    case TRADING_SESSION_STATUS:
      break;
    case MASS_QUOTE:
      break;
    case BUSINESS_MESSAGE_REJECT:
      break;
    case BID_REQUEST:
      break;
    case BID_RESPONSE:
      break;
    case LIST_STRIKE_PRICE:
      break;
    case XML_NON_FIX:
      break;
    case REGISTRATION_INSTRUCTIONS:
      break;
    case REGISTRATION_INSTRUCTIONS_RESPONSE:
      break;
    case ORDER_MASS_CANCEL_REQUEST:
      break;
    case ORDER_MASS_CANCEL_REPORT:
      break;
    case NEW_ORDER_CROSS:
      break;
    case CROSS_ORDER_CANCEL_REPLACE_REQUEST:
      break;
    case CROSS_ORDER_CANCEL_REQUEST:
      break;
    case SECURITY_TYPE_REQUEST:
      break;
    case SECURITY_TYPES:
      break;
    case SECURITY_LIST_REQUEST:
      dispatch(subscriptions_.security_req_id);
      return;  // note!
    case SECURITY_LIST:
      break;
    case DERIVATIVE_SECURITY_LIST_REQUEST:
      break;
  }
  // note! must be an internal issue
}

void Controller::operator()(Trace<codec::fix::UserResponse> const &event) {
  auto &user_response = event.value;
  auto iter = subscriptions_.user.server_to_client.find(user_response.user_request_id);
  if (iter != std::end(subscriptions_.user.server_to_client)) {
    auto session_id = (*iter).second;
    if (client_manager_.find(session_id, [&](auto &session) {
          switch (user_response.user_status) {
            using enum roq::fix::UserStatus;
            case LOGGED_IN:
              user_add(user_response.username, session_id);
              break;
            case NOT_LOGGED_IN:
              user_remove(user_response.username, session.ready());
              break;
            default:
              log::warn("Unexpected: user_response={}"sv, user_response);
          }
          subscriptions_.user.client_to_server.erase(session_id);
          subscriptions_.user.server_to_client.erase(iter);
          session(event);
        })) {
    } else {
      // note! clean up whatever the response
      user_remove(user_response.username, false);
    }
  } else {
    log::fatal("Unexpected"sv);
  }
}

void Controller::operator()(Trace<codec::fix::SecurityList> const &event) {
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, auto keep_alive) {
    auto failure = event.value.security_request_result != roq::fix::SecurityRequestResult::VALID;
    remove = failure || !keep_alive;
    auto security_list = event.value;
    security_list.security_req_id = req_id;
    Trace event_2{event.trace_info, security_list};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.security_req_id;
  auto &mapping = subscriptions_.security_req_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::SecurityDefinition> const &event) {
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, auto keep_alive) {
    auto failure = event.value.security_response_type != roq::fix::SecurityResponseType::ACCEPT_SECURITY_PROPOSAL_AS_IS;
    remove = failure || !keep_alive;
    auto security_definition = event.value;
    security_definition.security_req_id = req_id;
    Trace event_2{event.trace_info, security_definition};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.security_req_id;
  auto &mapping = subscriptions_.security_req_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::SecurityStatus> const &event) {
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, auto keep_alive) {
    // note! there is not way to detect a reject
    remove = !keep_alive;
    auto security_status = event.value;
    security_status.security_status_req_id = req_id;
    Trace event_2{event.trace_info, security_status};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.security_status_req_id;
  auto &mapping = subscriptions_.security_status_req_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::MarketDataRequestReject> const &event) {
  auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
    auto market_data_request_reject = event.value;
    market_data_request_reject.md_req_id = req_id;
    Trace event_2{event.trace_info, market_data_request_reject};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.md_req_id;
  auto &mapping = subscriptions_.md_req_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::MarketDataSnapshotFullRefresh> const &event) {
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, auto keep_alive) {
    remove = !keep_alive;
    auto market_data_snapshot_full_refresh = event.value;
    market_data_snapshot_full_refresh.md_req_id = req_id;
    Trace event_2{event.trace_info, market_data_snapshot_full_refresh};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.md_req_id;
  auto &mapping = subscriptions_.md_req_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::MarketDataIncrementalRefresh> const &event) {
  auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
    auto market_data_incremental_refresh = event.value;
    market_data_incremental_refresh.md_req_id = req_id;
    Trace event_2{event.trace_info, market_data_incremental_refresh};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.md_req_id;
  auto &mapping = subscriptions_.md_req_id;
  find_req_id(mapping, req_id, dispatch);
  // note! delivery failure is valid (an unsubscribe request could already have removed md_req_id)
}

void Controller::operator()(Trace<codec::fix::OrderCancelReject> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::OrderMassCancelReport> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::ExecutionReport> const &event) {
  auto has_ord_status_req_id = !std::empty(event.value.ord_status_req_id);
  auto has_mass_status_req_id = !std::empty(event.value.mass_status_req_id);
  assert(!(has_ord_status_req_id && has_mass_status_req_id));  // can't have both
  if (has_ord_status_req_id) {
    auto &req_id = event.value.ord_status_req_id;
    auto &mapping = subscriptions_.ord_status_req_id;
    auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
      auto execution_report = event.value;
      execution_report.ord_status_req_id = req_id;
      Trace event_2{event.trace_info, execution_report};
      dispatch_to_client(event_2, session_id);
    };
    if (find_req_id(mapping, req_id, dispatch)) {
      assert(event.value.last_rpt_requested);
      remove_req_id(mapping, req_id);
    }
  } else if (has_mass_status_req_id) {
    auto &req_id = event.value.mass_status_req_id;
    auto &mapping = subscriptions_.mass_status_req_id;
    auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
      auto execution_report = event.value;
      execution_report.mass_status_req_id = req_id;
      Trace event_2{event.trace_info, execution_report};
      dispatch_to_client(event_2, session_id);
    };
    if (find_req_id(mapping, req_id, dispatch)) {
      if (event.value.last_rpt_requested)
        remove_req_id(mapping, req_id);
    }
  } else {
    auto client_id = get_client_from_parties(event.value);
    broadcast(event, client_id);
  }
}

void Controller::operator()(Trace<codec::fix::RequestForPositionsAck> const &event) {
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
    auto failure = event.value.pos_req_result != roq::fix::PosReqResult::VALID ||
                   event.value.pos_req_status == roq::fix::PosReqStatus::REJECTED;
    if (failure) {
      remove = true;
      total_num_pos_reports_ = {};
    } else {
      remove = false;
      total_num_pos_reports_ = event.value.total_num_pos_reports;
    }
    auto position_report = event.value;
    position_report.pos_req_id = req_id;
    Trace event_2{event.trace_info, position_report};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.pos_req_id;
  auto &mapping = subscriptions_.pos_req_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::PositionReport> const &event) {
  if (total_num_pos_reports_)
    --total_num_pos_reports_;
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, auto keep_alive) {
    auto failure = event.value.pos_req_result != roq::fix::PosReqResult::VALID;
    if (failure) {
      remove = true;
    } else if (!total_num_pos_reports_) {
      remove = !keep_alive;
    }
    auto position_report = event.value;
    position_report.pos_req_id = req_id;
    Trace event_2{event.trace_info, position_report};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.pos_req_id;
  auto &mapping = subscriptions_.pos_req_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::TradeCaptureReportRequestAck> const &event) {
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, auto keep_alive) {
    remove = !keep_alive;
    auto trade_capture_report_request_ack = event.value;
    trade_capture_report_request_ack.trade_request_id = req_id;
    Trace event_2{event.trace_info, trade_capture_report_request_ack};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.trade_request_id;
  auto &mapping = subscriptions_.trade_request_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

void Controller::operator()(Trace<codec::fix::TradeCaptureReport> const &event) {
  auto remove = true;
  auto dispatch = [&](auto session_id, auto &req_id, auto keep_alive) {
    if (!event.value.last_rpt_requested)
      remove = false;
    else
      remove = !keep_alive;
    auto trade_capture_report = event.value;
    trade_capture_report.trade_request_id = req_id;
    Trace event_2{event.trace_info, trade_capture_report};
    dispatch_to_client(event_2, session_id);
  };
  auto &req_id = event.value.trade_request_id;
  auto &mapping = subscriptions_.trade_request_id;
  if (find_req_id(mapping, req_id, dispatch)) {
    if (remove)
      remove_req_id(mapping, req_id);
  } else {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
  }
}

// client::Session::Handler

void Controller::operator()(Trace<client::Session::Disconnected> const &event, uint64_t session_id) {
  // market data
  auto iter_1 = subscriptions_.md_req_id.client_to_server.find(session_id);
  if (iter_1 != std::end(subscriptions_.md_req_id.client_to_server)) {
    for (auto &[_, server_md_req_id] : (*iter_1).second) {
      if (ready()) {
        auto market_data_request = codec::fix::MarketDataRequest{
            .md_req_id = server_md_req_id,
            .subscription_request_type = roq::fix::SubscriptionRequestType::UNSUBSCRIBE,
            .market_depth = {},
            .md_update_type = {},
            .aggregated_book = {},
            .no_md_entry_types = {},  // note! non-standard -- fix-bridge will unsubscribe all
            .no_related_sym = {},
            .no_trading_sessions = {},
            .custom_type = {},
            .custom_value = {},
        };
        Trace event_2{event.trace_info, market_data_request};
        dispatch_to_server(event_2);
      }
      subscriptions_.md_req_id.server_to_client.erase(server_md_req_id);
    }
    subscriptions_.md_req_id.client_to_server.erase(iter_1);
  } else {
    log::debug("no market data subscriptions associated with session_id={}"sv, session_id);
  }
  // user
  auto iter_2 = subscriptions_.user.session_to_client.find(session_id);
  if (iter_2 != std::end(subscriptions_.user.session_to_client)) {
    auto &username_2 = (*iter_2).second;
    if (ready()) {
      auto user_request_id = shared_.create_request_id();
      auto user_request = codec::fix::UserRequest{
          .user_request_id = user_request_id,
          .user_request_type = roq::fix::UserRequestType::LOG_OFF_USER,
          .username = username_2,
          .password = {},
          .new_password = {},
      };
      Trace event_2{event.trace_info, user_request};
      dispatch_to_server(event_2);
      subscriptions_.user.server_to_client.try_emplace(user_request.user_request_id, session_id);
      subscriptions_.user.client_to_server.try_emplace(session_id, user_request.user_request_id);
    }
    // note!
    // there are two scenarios:
    //   we can't send ==> fix-bridge is disconnected so it doesn't matter
    //   we get a response => fix-bridge was connect and we expect it to do the right thing
    // therefore: release immediately to allow the client to reconnect
    log::debug(R"(USER REMOVE client_id="{}" <==> session_id={})"sv, username_2, session_id);
    subscriptions_.user.client_to_session.erase((*iter_2).second);
    subscriptions_.user.session_to_client.erase(iter_2);
  } else {
    log::debug("no user associated with session_id={}"sv, session_id);
  }
}

void Controller::operator()(Trace<codec::fix::UserRequest> const &event, uint64_t session_id) {
  auto &user_request = event.value;
  switch (user_request.user_request_type) {
    using enum roq::fix::UserRequestType;
    case LOG_ON_USER:
      if (user_is_locked(user_request.username))
        throw oms::Rejected{Origin::CLIENT, Error::UNKNOWN, "locked"sv};
      break;
    case LOG_OFF_USER:
      break;
    default:
      log::fatal("Unexpected: user_request={}"sv, user_request);
  }
  auto &tmp = subscriptions_.user.client_to_server[session_id];
  if (std::empty(tmp)) {
    tmp = user_request.user_request_id;
    auto res = subscriptions_.user.server_to_client.try_emplace(user_request.user_request_id, session_id).second;
    assert(res);
    dispatch_to_server(event);
  } else {
    log::fatal("Unexpected"sv);
  }
}

void Controller::operator()(Trace<codec::fix::SecurityListRequest> const &event, uint64_t session_id) {
  auto &req_id = event.value.security_req_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.security_req_id;
  auto reject = [&]() {
    auto request_id = shared_.create_request_id();
    auto security_list = roq::codec::fix::SecurityList{
        .security_req_id = req_id,
        .security_response_id = request_id,
        .security_request_result = roq::fix::SecurityRequestResult::INVALID_OR_UNSUPPORTED,
        .no_related_sym = {},
    };
    Trace event_2{event.trace_info, security_list};
    dispatch_to_client(event_2, session_id);
  };
  auto &client_to_server = mapping.client_to_server[session_id];
  auto iter = client_to_server.find(req_id);
  auto exists = iter != std::end(client_to_server);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto security_list_request = event.value;
    security_list_request.security_req_id = request_id;
    Trace event_2{event.trace_info, security_list_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(event.value.subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (event.value.subscription_request_type) {
    using enum roq::fix::SubscriptionRequestType;
    case UNDEFINED:
    case UNKNOWN:
      reject();
      break;
    case SNAPSHOT:
      if (exists) {
        reject();
      } else {
        dispatch(false);
      }
      break;
    case SNAPSHOT_UPDATES:
      if (exists) {
        reject();
      } else {
        dispatch(true);
      }
      break;
    case UNSUBSCRIBE:
      if (exists) {
        dispatch(false);
      } else {
        reject();
      }
      break;
  }
}

void Controller::operator()(Trace<codec::fix::SecurityDefinitionRequest> const &event, uint64_t session_id) {
  auto &req_id = event.value.security_req_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.security_req_id;
  auto reject = [&]() {
    auto &security_definition_request = event.value;
    auto request_id = shared_.create_request_id();
    auto security_definition = codec::fix::SecurityDefinition{
        .security_req_id = security_definition_request.security_req_id,
        .security_response_id = request_id,
        .security_response_type = roq::fix::SecurityResponseType::REJECT_SECURITY_PROPOSAL,
        .symbol = security_definition_request.symbol,
        .contract_multiplier = {},
        .security_exchange = security_definition_request.security_exchange,
        .trading_session_id = {},
        .min_trade_vol = {},
    };
    Trace event_2{event.trace_info, security_definition};
    dispatch_to_client(event_2, session_id);
  };
  auto &client_to_server = mapping.client_to_server[session_id];
  auto iter = client_to_server.find(req_id);
  auto exists = iter != std::end(client_to_server);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto security_definition_request = event.value;
    security_definition_request.security_req_id = request_id;
    Trace event_2{event.trace_info, security_definition_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(event.value.subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (event.value.subscription_request_type) {
    using enum roq::fix::SubscriptionRequestType;
    case UNDEFINED:
    case UNKNOWN:
      reject();
      break;
    case SNAPSHOT:
      if (exists) {
        reject();
      } else {
        dispatch(false);
      }
      break;
    case SNAPSHOT_UPDATES:
      if (exists) {
        reject();
      } else {
        dispatch(true);
      }
      break;
    case UNSUBSCRIBE:
      if (exists) {
        dispatch(false);
      } else {
        reject();
      }
      break;
  }
}

void Controller::operator()(Trace<codec::fix::SecurityStatusRequest> const &event, uint64_t session_id) {
  auto &req_id = event.value.security_status_req_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.security_status_req_id;
  auto reject = [&]() {
    auto &security_status_request = event.value;
    // note! protocol doesn't have a proper solution for reject
    auto security_status = codec::fix::SecurityStatus{
        .security_status_req_id = security_status_request.security_status_req_id,
        .symbol = security_status_request.symbol,
        .security_exchange = security_status_request.security_exchange,
        .trading_session_id = {},
        .unsolicited_indicator = false,
        .security_trading_status = {},
    };
    Trace event_2{event.trace_info, security_status};
    dispatch_to_client(event_2, session_id);
  };
  auto &client_to_server = mapping.client_to_server[session_id];
  auto iter = client_to_server.find(req_id);
  auto exists = iter != std::end(client_to_server);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto security_definition_request = event.value;
    security_definition_request.security_status_req_id = request_id;
    Trace event_2{event.trace_info, security_definition_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(event.value.subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (event.value.subscription_request_type) {
    using enum roq::fix::SubscriptionRequestType;
    case UNDEFINED:
    case UNKNOWN:
      reject();
      break;
    case SNAPSHOT:
      if (exists) {
        reject();
      } else {
        dispatch(false);
      }
      break;
    case SNAPSHOT_UPDATES:
      if (exists) {
        reject();
      } else {
        dispatch(true);
      }
      break;
    case UNSUBSCRIBE:
      if (exists) {
        dispatch(false);
      } else {
        reject();
      }
      break;
  }
}

void Controller::operator()(Trace<codec::fix::MarketDataRequest> const &event, uint64_t session_id) {
  auto &req_id = event.value.md_req_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.md_req_id;
  auto reject = [&](auto md_req_rej_reason, auto const &text) {
    auto market_data_request_reject = roq::codec::fix::MarketDataRequestReject{
        .md_req_id = req_id,
        .md_req_rej_reason = md_req_rej_reason,
        .text = text,
    };
    Trace event_2{event.trace_info, market_data_request_reject};
    dispatch_to_client(event_2, session_id);
  };
  auto &client_to_server = mapping.client_to_server[session_id];
  auto iter = client_to_server.find(req_id);
  auto exists = iter != std::end(client_to_server);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto market_data_request = event.value;
    market_data_request.md_req_id = request_id;
    Trace event_2{event.trace_info, market_data_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(event.value.subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (event.value.subscription_request_type) {
    using enum roq::fix::SubscriptionRequestType;
    case UNDEFINED:
    case UNKNOWN:
      reject(roq::fix::MDReqRejReason::UNSUPPORTED_SUBSCRIPTION_REQUEST_TYPE, "UNKNOWN_SUBSCRIPTION_REQUEST_TYPE"sv);
      break;
    case SNAPSHOT:
      if (exists) {
        reject(roq::fix::MDReqRejReason::DUPLICATE_MD_REQ_ID, "DUPLICATE_MD_REQ_ID"sv);
      } else {
        dispatch(false);
      }
      break;
    case SNAPSHOT_UPDATES:
      if (exists) {
        reject(roq::fix::MDReqRejReason::DUPLICATE_MD_REQ_ID, "DUPLICATE_MD_REQ_ID"sv);
      } else {
        dispatch(true);
      }
      break;
    case UNSUBSCRIBE:
      if (exists) {
        dispatch(false);
      } else {
        reject(roq::fix::MDReqRejReason::UNSUPPORTED_SUBSCRIPTION_REQUEST_TYPE, "UNKNOWN_MD_REQ_ID"sv);
      }
      break;
  }
}

void Controller::operator()(Trace<codec::fix::OrderStatusRequest> const &event, uint64_t session_id) {
  auto &req_id = event.value.ord_status_req_id;
  auto &mapping = subscriptions_.ord_status_req_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto ord_rej_reason, auto const &text) {
    auto &order_status_request = event.value;
    auto request_id = shared_.create_request_id();
    auto execution_report = codec::fix::ExecutionReport{
        .order_id = request_id,  // required
        .cl_ord_id = order_status_request.cl_ord_id,
        .orig_cl_ord_id = {},
        .ord_status_req_id = order_status_request.ord_status_req_id,
        .mass_status_req_id = {},
        .tot_num_reports = 0,  // note!
        .last_rpt_requested = true,
        .no_party_ids = order_status_request.no_party_ids,
        .exec_id = request_id,                          // required
        .exec_type = roq::fix::ExecType::ORDER_STATUS,  // required
        .ord_status = roq::fix::OrdStatus::REJECTED,    // required
        .working_indicator = {},
        .ord_rej_reason = ord_rej_reason,  // note!
        .account = order_status_request.account,
        .account_type = {},
        .symbol = order_status_request.symbol,                        // required
        .security_exchange = order_status_request.security_exchange,  // note! quickfix
        .side = order_status_request.side,                            // required
        .order_qty = {},
        .price = {},
        .stop_px = {},
        .currency = {},
        .time_in_force = {},
        .exec_inst = {},
        .last_qty = {},
        .last_px = {},
        .trading_session_id = {},
        .leaves_qty = {},
        .cum_qty = {},
        .avg_px = {},
        .transact_time = {},
        .position_effect = {},
        .max_show = {},
        .text = text,
        .last_liquidity_ind = {},
    };
    Trace event_2{event.trace_info, execution_report};
    dispatch_to_client(event_2, session_id);
  };
  auto dispatch = [&]() {
    auto request_id = shared_.create_request_id();
    auto order_status_request = event.value;
    order_status_request.ord_status_req_id = request_id;
    Trace event_2{event.trace_info, order_status_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (!std::empty(req_id))  // optional
      client_to_server.emplace(req_id, request_id);
    mapping.server_to_client.try_emplace(request_id, session_id, req_id, false);
  };
  if (std::empty(req_id)) {  // optional
    dispatch();
  } else {
    auto iter = client_to_server.find(req_id);
    if (iter == std::end(client_to_server)) {
      dispatch();
    } else {
      reject(roq::fix::OrdRejReason::OTHER, "DUPLICATE_ORD_STATUS_REQ_ID"sv);
    }
  }
}

void Controller::operator()(Trace<codec::fix::NewOrderSingle> const &event, uint64_t session_id) {
  dispatch_to_server(event);
}

void Controller::operator()(Trace<codec::fix::OrderCancelReplaceRequest> const &event, uint64_t session_id) {
  dispatch_to_server(event);
}

void Controller::operator()(Trace<codec::fix::OrderCancelRequest> const &event, uint64_t session_id) {
  dispatch_to_server(event);
}

void Controller::operator()(Trace<codec::fix::OrderMassStatusRequest> const &event, uint64_t session_id) {
  auto &req_id = event.value.mass_status_req_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.ord_status_req_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto ord_rej_reason, auto const &text) {
    auto &order_mass_status_request = event.value;
    auto request_id = shared_.create_request_id();
    auto execution_report = codec::fix::ExecutionReport{
        .order_id = request_id,  // required
        .cl_ord_id = {},
        .orig_cl_ord_id = {},
        .ord_status_req_id = {},
        .mass_status_req_id = order_mass_status_request.mass_status_req_id,
        .tot_num_reports = 0,  // note!
        .last_rpt_requested = true,
        .no_party_ids = order_mass_status_request.no_party_ids,
        .exec_id = request_id,                          // required
        .exec_type = roq::fix::ExecType::ORDER_STATUS,  // required
        .ord_status = roq::fix::OrdStatus::REJECTED,    // required
        .working_indicator = {},
        .ord_rej_reason = ord_rej_reason,  // note!
        .account = order_mass_status_request.account,
        .account_type = {},
        .symbol = order_mass_status_request.symbol,                        // required
        .security_exchange = order_mass_status_request.security_exchange,  // note! quickfix
        .side = order_mass_status_request.side,                            // required
        .order_qty = {},
        .price = {},
        .stop_px = {},
        .currency = {},
        .time_in_force = {},
        .exec_inst = {},
        .last_qty = {},
        .last_px = {},
        .trading_session_id = {},
        .leaves_qty = {},
        .cum_qty = {},
        .avg_px = {},
        .transact_time = {},
        .position_effect = {},
        .max_show = {},
        .text = text,
        .last_liquidity_ind = {},
    };
    Trace event_2{event.trace_info, execution_report};
    dispatch_to_client(event_2, session_id);
  };
  auto dispatch = [&]() {
    auto request_id = shared_.create_request_id();
    auto order_mass_status_request = event.value;
    order_mass_status_request.mass_status_req_id = request_id;
    Trace event_2{event.trace_info, order_mass_status_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    client_to_server.emplace(req_id, request_id);
    mapping.server_to_client.try_emplace(request_id, session_id, req_id, false);
  };
  auto iter = client_to_server.find(req_id);
  if (iter == std::end(client_to_server)) {
    dispatch();
  } else {
    reject(roq::fix::OrdRejReason::OTHER, "DUPLICATE_MASS_STATUS_REQ_ID"sv);
  }
}

void Controller::operator()(Trace<codec::fix::OrderMassCancelRequest> const &event, uint64_t session_id) {
  dispatch_to_server(event);
}

void Controller::operator()(Trace<codec::fix::RequestForPositions> const &event, uint64_t session_id) {
  auto &req_id = event.value.pos_req_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.pos_req_id;
  auto reject = [&](auto const &text) {
    auto &request_for_positions = event.value;
    auto request_id = shared_.create_request_id();
    auto request_for_positions_ack = codec::fix::RequestForPositionsAck{
        .pos_maint_rpt_id = request_id,  // required
        .pos_req_id = req_id,
        .total_num_pos_reports = {},
        .unsolicited_indicator = false,
        .pos_req_result = roq::fix::PosReqResult::INVALID_OR_UNSUPPORTED,  // required
        .pos_req_status = roq::fix::PosReqStatus::REJECTED,                // required
        .no_party_ids = request_for_positions.no_party_ids,                // required
        .account = request_for_positions.account,                          // required
        .account_type = request_for_positions.account_type,                // required
        .text = text,
    };
    log::debug("request_for_positions_ack={}"sv, request_for_positions_ack);
    Trace event_2{event.trace_info, request_for_positions_ack};
    dispatch_to_client(event_2, session_id);
  };
  auto &client_to_server = mapping.client_to_server[session_id];
  auto iter = client_to_server.find(req_id);
  auto exists = iter != std::end(client_to_server);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto request_for_positions = event.value;
    request_for_positions.pos_req_id = request_id;
    Trace event_2{event.trace_info, request_for_positions};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(event.value.subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (event.value.subscription_request_type) {
    using enum roq::fix::SubscriptionRequestType;
    case UNDEFINED:
    case UNKNOWN:
      reject("INVALID"sv);
      break;
    case SNAPSHOT:
      if (exists) {
        reject("DUPLICATED_POS_REQ_ID"sv);
      } else {
        dispatch(false);
      }
      break;
    case SNAPSHOT_UPDATES:
      if (exists) {
        reject("DUPLICATED_POS_REQ_ID"sv);
      } else {
        dispatch(true);
      }
      break;
    case UNSUBSCRIBE:
      if (exists) {
        dispatch(false);
      } else {
        reject("UNKNOWN_POS_REQ_ID"sv);
      }
      break;
  }
}

void Controller::operator()(Trace<codec::fix::TradeCaptureReportRequest> const &event, uint64_t session_id) {
  auto &req_id = event.value.trade_request_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.trade_request_id;
  auto reject = [&]() {
    auto &trade_capture_report_request = event.value;
    auto request_id = shared_.create_request_id();
    auto trade_capture_report_request_ack = codec::fix::TradeCaptureReportRequestAck{
        .trade_request_id = req_id,                                             // required
        .trade_request_type = trade_capture_report_request.trade_request_type,  // required
        .trade_request_result = roq::fix::TradeRequestResult::OTHER,            // required
        .trade_request_status = roq::fix::TradeRequestStatus::REJECTED,         // required
        .symbol = trade_capture_report_request.symbol,                          // required
        .security_exchange = trade_capture_report_request.security_exchange,    // required
        .text = {},
    };
    Trace event_2{event.trace_info, trade_capture_report_request_ack};
    dispatch_to_client(event_2, session_id);
  };
  auto &client_to_server = mapping.client_to_server[session_id];
  auto iter = client_to_server.find(req_id);
  auto exists = iter != std::end(client_to_server);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto trade_capture_report_request = event.value;
    trade_capture_report_request.trade_request_id = request_id;
    Trace event_2{event.trace_info, trade_capture_report_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(event.value.subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          event.value.subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (event.value.subscription_request_type) {
    using enum roq::fix::SubscriptionRequestType;
    case UNDEFINED:
    case UNKNOWN:
      reject();
      break;
    case SNAPSHOT:
      if (exists) {
        reject();
      } else {
        dispatch(false);
      }
      break;
    case SNAPSHOT_UPDATES:
      if (exists) {
        reject();
      } else {
        dispatch(true);
      }
      break;
    case UNSUBSCRIBE:
      if (exists) {
        dispatch(false);
      } else {
        reject();
      }
      break;
  }
}

// utilities

template <typename... Args>
void Controller::dispatch(Args &&...args) {
  auto message_info = MessageInfo{};
  Event event{message_info, std::forward<Args>(args)...};
  server_session_(event);
  client_manager_(event);
}

template <typename T>
void Controller::dispatch_to_server(Trace<T> const &event) {
  server_session_(event);
}

template <typename T>
void Controller::dispatch_to_client(Trace<T> const &event, std::string_view const &username) {
  // [[maybe_unused]] auto strategy_id = get_strategy_id(event.value);
  auto success = false;
  shared_.session_find(username, [&](auto session_id) {
    client_manager_.find(session_id, [&](auto &session) {
      session(event);
      success = true;
    });
  });
  if (!success)
    log::warn<0>(R"(Undeliverable: username="{}")"sv, username);
}

template <typename T>
bool Controller::dispatch_to_client(Trace<T> const &event, uint64_t session_id) {
  auto success = false;
  client_manager_.find(session_id, [&](auto &session) {
    session(event);
    success = true;
  });
  if (!success)
    log::warn<0>("Undeliverable: session_id={}"sv, session_id);
  return success;
}

template <typename T>
void Controller::broadcast(Trace<T> const &event, std::string_view const &client_id) {
  auto iter = subscriptions_.user.client_to_session.find(client_id);
  if (iter == std::end(subscriptions_.user.client_to_session))
    return;
  auto session_id = (*iter).second;
  client_manager_.find(session_id, [&](auto &session) { session(event); });
}

// mapping

template <typename Callback>
bool Controller::find_req_id(auto &mapping, std::string_view const &req_id, Callback callback) {
  auto iter = mapping.server_to_client.find(req_id);
  if (iter == std::end(mapping.server_to_client))
    return false;
  auto &[session_id, client_req_id, keep_alive] = (*iter).second;
  callback(session_id, client_req_id, keep_alive);
  return true;
}

void Controller::remove_req_id(auto &mapping, std::string_view const &req_id) {
  auto iter_1 = mapping.server_to_client.find(req_id);
  if (iter_1 == std::end(mapping.server_to_client)) {
    log::warn(R"(Internal error: req_id="{}")"sv, req_id);
    return;
  }
  auto &[session_id, client_req_id, keep_alive] = (*iter_1).second;
  auto iter_2 = mapping.client_to_server.find(session_id);
  if (iter_2 != std::end(mapping.client_to_server)) {
    (*iter_2).second.erase(client_req_id);
    if (std::empty((*iter_2).second))
      mapping.client_to_server.erase(iter_2);
  }
  mapping.server_to_client.erase(iter_1);
}

// user

void Controller::user_add(std::string_view const &username, uint64_t session_id) {
  log::info(R"(DEBUG: USER ADD client_id="{}" <==> session_id={})"sv, username, session_id);
  auto res_1 = subscriptions_.user.client_to_session.try_emplace(username, session_id).second;
  if (!res_1)
    log::fatal("Unexpected"sv);
  auto res_2 = subscriptions_.user.session_to_client.try_emplace(session_id, username).second;
  if (!res_2)
    log::fatal("Unexpected"sv);
}

void Controller::user_remove(std::string_view const &username, bool ready) {
  auto iter = subscriptions_.user.client_to_session.find(username);
  if (iter != std::end(subscriptions_.user.client_to_session)) {
    auto session_id = (*iter).second;
    log::info(R"(DEBUG: USER REMOVE client_id="{}" <==> session_id={})"sv, username, session_id);
    subscriptions_.user.session_to_client.erase(session_id);
    subscriptions_.user.client_to_session.erase(iter);
  } else if (ready) {
    // note! disconnect doesn't wait before cleaning up the resources
    log::fatal(R"(Unexpected: client_id="{}")"sv, username);
  }
}

bool Controller::user_is_locked(std::string_view const &username) const {
  auto iter = subscriptions_.user.client_to_session.find(username);
  return iter != std::end(subscriptions_.user.client_to_session);
}

}  // namespace fix
}  // namespace proxy
}  // namespace roq
