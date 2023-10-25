/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/controller.hpp"

#include "roq/event.hpp"
#include "roq/timer.hpp"

#include "roq/utils/update.hpp"

#include "roq/oms/exceptions.hpp"

#include "roq/fix/utils.hpp"

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

auto find_real_cl_ord_id(auto &cl_ord_id) -> std::string_view {
  auto pos = cl_ord_id.find(':');
  return pos == cl_ord_id.npos ? cl_ord_id : cl_ord_id.substr(pos + 1);
}

auto is_order_complete(auto ord_status) {
  auto order_status = roq::fix::map(ord_status);
  return roq::utils::is_order_complete(order_status);
}

auto get_subscription_request_type(auto &event) {
  auto result = event.value.subscription_request_type;
  if (result == roq::fix::SubscriptionRequestType::UNDEFINED)
    result = roq::fix::SubscriptionRequestType::SNAPSHOT;
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
  // XXX FIXME clear cl_ord_id_ ???
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
  auto req_id = event.value.security_req_id;
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
  auto req_id = event.value.security_req_id;
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
  auto req_id = event.value.security_status_req_id;
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
  auto req_id = event.value.md_req_id;
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
  auto req_id = event.value.md_req_id;
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
  auto req_id = event.value.md_req_id;
  auto &mapping = subscriptions_.md_req_id;
  find_req_id(mapping, req_id, dispatch);
  // note! delivery failure is valid (an unsubscribe request could already have removed md_req_id)
}

void Controller::operator()(Trace<codec::fix::OrderCancelReject> const &event) {
  auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
    auto orig_cl_ord_id = find_real_cl_ord_id(event.value.orig_cl_ord_id);
    auto order_cancel_reject = event.value;
    order_cancel_reject.cl_ord_id = req_id;
    order_cancel_reject.orig_cl_ord_id = orig_cl_ord_id;
    Trace event_2{event.trace_info, order_cancel_reject};
    dispatch_to_client(event_2, session_id);
  };
  auto req_id = event.value.cl_ord_id;
  auto &mapping = subscriptions_.cl_ord_id;
  find_req_id(mapping, req_id, dispatch);
  remove_req_id(mapping, req_id);
}

void Controller::operator()(Trace<codec::fix::OrderMassCancelReport> const &event) {
  auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
    auto order_mass_cancel_report = event.value;
    order_mass_cancel_report.cl_ord_id = req_id;
    Trace event_2{event.trace_info, order_mass_cancel_report};
    dispatch_to_client(event_2, session_id);
  };
  auto req_id = event.value.cl_ord_id;
  auto &mapping = subscriptions_.mass_cancel_cl_ord_id;
  find_req_id(mapping, req_id, dispatch);
  remove_req_id(mapping, req_id);
}

void Controller::operator()(Trace<codec::fix::ExecutionReport> const &event) {
  auto execution_report = event.value;
  auto cl_ord_id = execution_report.cl_ord_id;
  auto orig_cl_ord_id = execution_report.orig_cl_ord_id;
  execution_report.cl_ord_id = find_real_cl_ord_id(cl_ord_id);
  execution_report.orig_cl_ord_id = find_real_cl_ord_id(orig_cl_ord_id);
  auto has_ord_status_req_id = !std::empty(execution_report.ord_status_req_id);
  auto has_mass_status_req_id = !std::empty(execution_report.mass_status_req_id);
  assert(!(has_ord_status_req_id && has_mass_status_req_id));  // can't have both
  if (has_ord_status_req_id) {
    assert(execution_report.last_rpt_requested);
    auto req_id = execution_report.ord_status_req_id;
    auto &mapping = subscriptions_.ord_status_req_id;
    auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
      assert(std::empty(execution_report.orig_cl_ord_id));
      execution_report.ord_status_req_id = req_id;
      Trace event_2{event.trace_info, execution_report};
      dispatch_to_client(event_2, session_id);
    };
    if (find_req_id(mapping, req_id, dispatch)) {
      remove_req_id(mapping, req_id);
    } else {
      log::warn(R"(DEBUG: no ord_status_req_id="{}")"sv, req_id);
    }
  } else if (has_mass_status_req_id) {
    auto req_id = execution_report.mass_status_req_id;
    auto &mapping = subscriptions_.mass_status_req_id;
    auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
      execution_report.cl_ord_id = find_real_cl_ord_id(execution_report.cl_ord_id);
      assert(std::empty(execution_report.orig_cl_ord_id));
      execution_report.mass_status_req_id = req_id;
      Trace event_2{event.trace_info, execution_report};
      dispatch_to_client(event_2, session_id);
    };
    if (find_req_id(mapping, req_id, dispatch)) {
      if (execution_report.last_rpt_requested) {
        remove_req_id(mapping, req_id);
      }
    } else {
      log::warn(R"(DEBUG: no mass_status_req_id="{}")"sv, req_id);
    }
  } else {
    auto req_id = cl_ord_id;
    auto &mapping = subscriptions_.cl_ord_id;
    remove_req_id_relaxed(mapping, req_id);  // note! request, not routing
    auto client_id = get_client_from_parties(execution_report);
    assert(!std::empty(client_id));
    if (execution_report.exec_type == roq::fix::ExecType::REJECTED) {
      auto dispatch = [&](auto session_id, auto &req_id, [[maybe_unused]] auto keep_alive) {
        assert(execution_report.cl_ord_id == req_id);
        Trace event_2{event.trace_info, execution_report};
        dispatch_to_client(event_2, session_id);
      };
      find_req_id(mapping, req_id, dispatch);
    } else {
      auto done = is_order_complete(execution_report.ord_status);
      if (done) {
        remove_cl_ord_id(cl_ord_id, client_id);
      } else {
        ensure_cl_ord_id(cl_ord_id, client_id, execution_report.ord_status);
      }
      if (!std::empty(orig_cl_ord_id))
        remove_cl_ord_id(orig_cl_ord_id, client_id);
      Trace event_2{event.trace_info, execution_report};
      broadcast(event_2, client_id);
    }
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
      log::warn("Awaiting {} position reports..."sv, total_num_pos_reports_);
    }
    auto position_report = event.value;
    position_report.pos_req_id = req_id;
    Trace event_2{event.trace_info, position_report};
    dispatch_to_client(event_2, session_id);
  };
  auto req_id = event.value.pos_req_id;
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
  if (!total_num_pos_reports_)
    log::warn("... last position report!"sv);
  auto remove = false;
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
  auto req_id = event.value.pos_req_id;
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
  auto req_id = event.value.trade_request_id;
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
  auto req_id = event.value.trade_request_id;
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
  auto unsubscribe_market_data = [&](auto &req_id) {
    if (!ready())
      return;
    auto market_data_request = codec::fix::MarketDataRequest{
        .md_req_id = req_id,
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
  };
  clear_req_ids(subscriptions_.security_req_id, session_id);         // note! subscriptions not yet supported
  clear_req_ids(subscriptions_.security_status_req_id, session_id);  // note! subscriptions not yet supported
  clear_req_ids(subscriptions_.md_req_id, session_id, unsubscribe_market_data);
  clear_req_ids(subscriptions_.ord_status_req_id, session_id);
  clear_req_ids(subscriptions_.mass_status_req_id, session_id);
  clear_req_ids(subscriptions_.pos_req_id, session_id);        // note! subscriptions not yet supported
  clear_req_ids(subscriptions_.trade_request_id, session_id);  // note! subscriptions not yet supported
  clear_req_ids(subscriptions_.cl_ord_id, session_id);
  clear_req_ids(subscriptions_.mass_cancel_cl_ord_id, session_id);
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
  auto req_id = event.value.security_req_id;
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
  auto subscription_request_type = get_subscription_request_type(event);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto security_list_request = event.value;
    security_list_request.security_req_id = request_id;
    Trace event_2{event.trace_info, security_list_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (subscription_request_type) {
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
  auto req_id = event.value.security_req_id;
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
  auto subscription_request_type = get_subscription_request_type(event);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto security_definition_request = event.value;
    security_definition_request.security_req_id = request_id;
    Trace event_2{event.trace_info, security_definition_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (subscription_request_type) {
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
  auto req_id = event.value.security_status_req_id;
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
  auto subscription_request_type = get_subscription_request_type(event);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto security_definition_request = event.value;
    security_definition_request.security_status_req_id = request_id;
    Trace event_2{event.trace_info, security_definition_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (subscription_request_type) {
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
  auto req_id = event.value.md_req_id;
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
  auto req_id = event.value.ord_status_req_id;
  auto &mapping = subscriptions_.ord_status_req_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto ord_rej_reason, auto const &text) {
    auto &order_status_request = event.value;
    auto request_id = shared_.create_request_id();
    auto execution_report = codec::fix::ExecutionReport{
        .order_id = request_id,  // required
        .secondary_cl_ord_id = {},
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
  auto req_id = event.value.cl_ord_id;
  auto &mapping = subscriptions_.cl_ord_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto ord_rej_reason, auto const &text) {
    auto &new_order_single = event.value;
    auto request_id = shared_.create_request_id();
    auto execution_report = codec::fix::ExecutionReport{
        .order_id = request_id,  // required
        .secondary_cl_ord_id = {},
        .cl_ord_id = new_order_single.cl_ord_id,
        .orig_cl_ord_id = {},
        .ord_status_req_id = {},
        .mass_status_req_id = {},
        .tot_num_reports = {},
        .last_rpt_requested = {},
        .no_party_ids = new_order_single.no_party_ids,
        .exec_id = request_id,                          // required
        .exec_type = roq::fix::ExecType::ORDER_STATUS,  // required
        .ord_status = roq::fix::OrdStatus::REJECTED,    // required
        .working_indicator = {},
        .ord_rej_reason = ord_rej_reason,  // note!
        .account = new_order_single.account,
        .account_type = {},
        .symbol = new_order_single.symbol,                        // required
        .security_exchange = new_order_single.security_exchange,  // note! quickfix
        .side = new_order_single.side,                            // required
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
    auto request_id = shared_.create_request_id(event.value.cl_ord_id);
    auto new_order_single = event.value;
    new_order_single.cl_ord_id = request_id;
    Trace event_2{event.trace_info, new_order_single};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    client_to_server.emplace(req_id, request_id);
    mapping.server_to_client.try_emplace(request_id, session_id, req_id, true);
  };
  auto iter = client_to_server.find(req_id);
  if (iter == std::end(client_to_server)) {
    auto client_id = get_client_from_parties(event.value);
    auto cl_ord_id = find_server_cl_ord_id(event.value.cl_ord_id, client_id);
    if (std::empty(cl_ord_id)) {
      dispatch();
    } else {
      reject(roq::fix::OrdRejReason::OTHER, "DUPLICATE_ORD_STATUS_REQ_ID"sv);
    }
  } else {
    reject(roq::fix::OrdRejReason::OTHER, "DUPLICATE_ORD_STATUS_REQ_ID"sv);
  }
}

void Controller::operator()(Trace<codec::fix::OrderCancelReplaceRequest> const &event, uint64_t session_id) {
  auto req_id = event.value.cl_ord_id;
  auto &mapping = subscriptions_.cl_ord_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto ord_status, auto cxl_rej_reason, auto const &text) {
    auto &order_cancel_replace_request = event.value;
    auto order_cancel_reject = codec::fix::OrderCancelReject{
        .order_id = "NONE"sv,  // required
        .secondary_cl_ord_id = {},
        .cl_ord_id = order_cancel_replace_request.cl_ord_id,            // required
        .orig_cl_ord_id = order_cancel_replace_request.orig_cl_ord_id,  // required
        .ord_status = ord_status,                                       // required
        .working_indicator = {},
        .account = order_cancel_replace_request.account,
        .cxl_rej_response_to = roq::fix::CxlRejResponseTo::ORDER_CANCEL_REPLACE_REQUEST,
        .cxl_rej_reason = cxl_rej_reason,
        .text = text,
    };
    Trace event_2{event.trace_info, order_cancel_reject};
    dispatch_to_client(event_2, session_id);
  };
  auto dispatch = [&](auto &orig_cl_ord_id) {
    auto request_id = shared_.create_request_id(event.value.cl_ord_id);
    auto order_cancel_replace_request = event.value;
    order_cancel_replace_request.cl_ord_id = request_id;
    order_cancel_replace_request.orig_cl_ord_id = orig_cl_ord_id;
    Trace event_2{event.trace_info, order_cancel_replace_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    client_to_server.emplace(req_id, request_id);
    mapping.server_to_client.try_emplace(request_id, session_id, req_id, true);
  };
  // XXX TODO also check by-strategy routing table
  auto iter = client_to_server.find(req_id);
  if (iter == std::end(client_to_server)) {
    auto client_id = get_client_from_parties(event.value);
    auto orig_cl_ord_id = find_server_cl_ord_id(event.value.orig_cl_ord_id, client_id);
    if (!std::empty(orig_cl_ord_id)) {
      dispatch(orig_cl_ord_id);
    } else {
      reject(roq::fix::OrdStatus::REJECTED, roq::fix::CxlRejReason::UNKNOWN_ORDER, "UNKNOWN_ORIG_CL_ORD_ID"sv);
    }
  } else {
    reject(
        roq::fix::OrdStatus::REJECTED,  // XXX FIXME should be latest "known"
        roq::fix::CxlRejReason::DUPLICATE_CL_ORD_ID,
        "DUPLICATE_ORD_STATUS_REQ_ID"sv);
  }
}

void Controller::operator()(Trace<codec::fix::OrderCancelRequest> const &event, uint64_t session_id) {
  auto req_id = event.value.cl_ord_id;
  auto &mapping = subscriptions_.cl_ord_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto ord_status, auto cxl_rej_reason, auto const &text) {
    auto &order_cancel_request = event.value;
    auto order_cancel_reject = codec::fix::OrderCancelReject{
        .order_id = "NONE"sv,  // required
        .secondary_cl_ord_id = {},
        .cl_ord_id = order_cancel_request.cl_ord_id,            // required
        .orig_cl_ord_id = order_cancel_request.orig_cl_ord_id,  // required
        .ord_status = ord_status,                               // required
        .working_indicator = {},
        .account = order_cancel_request.account,
        .cxl_rej_response_to = roq::fix::CxlRejResponseTo::ORDER_CANCEL_REQUEST,
        .cxl_rej_reason = cxl_rej_reason,
        .text = text,
    };
    Trace event_2{event.trace_info, order_cancel_reject};
    dispatch_to_client(event_2, session_id);
  };
  auto dispatch = [&](auto &orig_cl_ord_id) {
    auto request_id = shared_.create_request_id(event.value.cl_ord_id);
    auto order_cancel_request = event.value;
    order_cancel_request.cl_ord_id = request_id;
    order_cancel_request.orig_cl_ord_id = orig_cl_ord_id;
    Trace event_2{event.trace_info, order_cancel_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    client_to_server.emplace(req_id, request_id);
    mapping.server_to_client.try_emplace(request_id, session_id, req_id, true);
  };
  auto iter = client_to_server.find(req_id);
  if (iter == std::end(client_to_server)) {
    auto client_id = get_client_from_parties(event.value);
    auto orig_cl_ord_id = find_server_cl_ord_id(event.value.orig_cl_ord_id, client_id);
    if (!std::empty(orig_cl_ord_id)) {
      dispatch(orig_cl_ord_id);
    } else {
      reject(roq::fix::OrdStatus::REJECTED, roq::fix::CxlRejReason::UNKNOWN_ORDER, "UNKNOWN_ORIG_CL_ORD_ID"sv);
    }
  } else {
    reject(
        roq::fix::OrdStatus::REJECTED,  // XXX FIXME should be latest "known"
        roq::fix::CxlRejReason::DUPLICATE_CL_ORD_ID,
        "DUPLICATE_ORD_STATUS_REQ_ID"sv);
  }
}

void Controller::operator()(Trace<codec::fix::OrderMassStatusRequest> const &event, uint64_t session_id) {
  auto req_id = event.value.mass_status_req_id;
  assert(!std::empty(req_id));  // required
  auto &mapping = subscriptions_.mass_status_req_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto ord_rej_reason, auto const &text) {
    auto &order_mass_status_request = event.value;
    auto request_id = shared_.create_request_id();
    auto execution_report = codec::fix::ExecutionReport{
        .order_id = request_id,  // required
        .secondary_cl_ord_id = {},
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
  auto req_id = event.value.cl_ord_id;
  auto &mapping = subscriptions_.mass_cancel_cl_ord_id;
  auto &client_to_server = mapping.client_to_server[session_id];
  auto reject = [&](auto order_mass_reject_reason, std::string_view const &text) {
    auto &order_mass_cancel_request = event.value;
    auto order_mass_cancel_report = codec::fix::OrderMassCancelReport{
        .cl_ord_id = order_mass_cancel_request.cl_ord_id,
        .order_id = order_mass_cancel_request.cl_ord_id,                                 // required
        .mass_cancel_request_type = order_mass_cancel_request.mass_cancel_request_type,  // required
        .mass_cancel_response = roq::fix::MassCancelResponse::CANCEL_REQUEST_REJECTED,   // required
        .mass_cancel_reject_reason = order_mass_reject_reason,
        .total_affected_orders = {},
        .symbol = order_mass_cancel_request.symbol,
        .security_exchange = order_mass_cancel_request.security_exchange,
        .side = order_mass_cancel_request.side,
        .text = text,
        .no_party_ids = order_mass_cancel_request.no_party_ids,
    };
    Trace event_2{event.trace_info, order_mass_cancel_report};
    dispatch_to_client(event_2, session_id);
  };
  auto dispatch = [&]() {
    auto request_id = shared_.create_request_id();
    auto order_mass_cancel_report = event.value;
    order_mass_cancel_report.cl_ord_id = request_id;
    Trace event_2{event.trace_info, order_mass_cancel_report};
    dispatch_to_server(event_2);
    client_to_server.emplace(req_id, request_id);
    mapping.server_to_client.try_emplace(request_id, session_id, req_id, false);
  };
  auto iter = client_to_server.find(req_id);
  if (iter == std::end(client_to_server)) {
    dispatch();
  } else {
    reject(roq::fix::MassCancelRejectReason::OTHER, "DUPLICATE_ORD_STATUS_REQ_ID"sv);
  }
}

void Controller::operator()(Trace<codec::fix::RequestForPositions> const &event, uint64_t session_id) {
  auto req_id = event.value.pos_req_id;
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
  auto subscription_request_type = get_subscription_request_type(event);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto request_for_positions = event.value;
    request_for_positions.pos_req_id = request_id;
    Trace event_2{event.trace_info, request_for_positions};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (subscription_request_type) {
    using enum roq::fix::SubscriptionRequestType;
    case UNDEFINED:
    case UNKNOWN:
      reject("UNKNOWN_SUBSCRIPTION_REQUEST_TYPE"sv);
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
  auto req_id = event.value.trade_request_id;
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
  auto subscription_request_type = get_subscription_request_type(event);
  auto dispatch = [&](auto keep_alive) {
    auto request_id = shared_.create_request_id();
    auto trade_capture_report_request = event.value;
    trade_capture_report_request.trade_request_id = request_id;
    Trace event_2{event.trace_info, trade_capture_report_request};
    dispatch_to_server(event_2);
    // note! *after* request has been sent
    if (exists) {
      assert(subscription_request_type == roq::fix::SubscriptionRequestType::UNSUBSCRIBE);
      remove_req_id(mapping, request_id);  // note! protocol doesn't have an ack for unsubscribe
    } else {
      assert(
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT ||
          subscription_request_type == roq::fix::SubscriptionRequestType::SNAPSHOT_UPDATES);
      client_to_server.emplace(req_id, request_id);
      mapping.server_to_client.try_emplace(request_id, session_id, req_id, keep_alive);
    }
  };
  switch (subscription_request_type) {
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
    log::warn(R"(DEBUG: REMOVE req_id(client)="{}")"sv, client_req_id);
    (*iter_2).second.erase(client_req_id);
    if (std::empty((*iter_2).second))
      mapping.client_to_server.erase(iter_2);
  }
  log::warn(R"(DEBUG: REMOVE req_id(server)="{}")"sv, req_id);
  mapping.server_to_client.erase(iter_1);
}

void Controller::remove_req_id_relaxed(auto &mapping, std::string_view const &req_id) {
  auto iter_1 = mapping.server_to_client.find(req_id);
  if (iter_1 == std::end(mapping.server_to_client))
    return;
  auto &[session_id, client_req_id, keep_alive] = (*iter_1).second;
  auto iter_2 = mapping.client_to_server.find(session_id);
  if (iter_2 != std::end(mapping.client_to_server)) {
    (*iter_2).second.erase(client_req_id);
    if (std::empty((*iter_2).second))
      mapping.client_to_server.erase(iter_2);
  }
  mapping.server_to_client.erase(iter_1);
}

template <typename Callback>
void Controller::clear_req_ids(auto &mapping, uint64_t session_id, Callback callback) {
  auto iter = mapping.client_to_server.find(session_id);
  if (iter == std::end(mapping.client_to_server))
    return;
  auto &tmp = (*iter).second;
  for (auto &[_, req_id] : tmp) {
    callback(req_id);
    mapping.server_to_client.erase(req_id);
  }
  mapping.client_to_server.erase(iter);
}

// cl_ord_id

void Controller::ensure_cl_ord_id(
    std::string_view const &cl_ord_id, std::string_view const &client_id, roq::fix::OrdStatus ord_status) {
  auto iter_1 = cl_ord_id_.state.find(cl_ord_id);
  if (iter_1 == std::end(cl_ord_id_.state)) {
    log::warn(R"(DEBUG: ADD cl_ord_id(server)="{}" ==> {})"sv, cl_ord_id, ord_status);
    auto res = cl_ord_id_.state.emplace(cl_ord_id, ord_status);
    assert(res.second);
  } else {
    if (utils::update((*iter_1).second, ord_status))
      log::warn(R"(DEBUG: UPDATE cl_ord_id(server)="{}" ==> {})"sv, cl_ord_id, ord_status);
  }
  auto real_cl_ord_id = find_real_cl_ord_id(cl_ord_id);
  assert(!std::empty(real_cl_ord_id));
  auto &tmp = cl_ord_id_.lookup[client_id];
  auto iter_2 = tmp.find(real_cl_ord_id);
  if (iter_2 == std::end(tmp)) {
    log::warn(
        R"(DEBUG: ADD {{client_id="{}", cl_ord_id(client)="{}"}} ==> cl_ord_id(server)="{}")"sv,
        client_id,
        real_cl_ord_id,
        cl_ord_id);
    auto res = tmp.emplace(real_cl_ord_id, cl_ord_id);
    assert(res.second);
  } else {
    assert((*iter_2).second == cl_ord_id);
  }
}

void Controller::remove_cl_ord_id(std::string_view const &cl_ord_id, std::string_view const &client_id) {
  if (shared_.settings.test.disable_remove_cl_ord_id)
    return;
  auto iter_1 = cl_ord_id_.state.find(cl_ord_id);
  if (iter_1 != std::end(cl_ord_id_.state)) {
    log::warn(R"(DEBUG: REMOVE cl_ord_id(server)="{}")"sv, cl_ord_id);
    cl_ord_id_.state.erase(iter_1);
  }
  auto iter_2 = cl_ord_id_.lookup.find(client_id);
  if (iter_2 != std::end(cl_ord_id_.lookup)) {
    auto &tmp = (*iter_2).second;
    auto real_cl_ord_id = find_real_cl_ord_id(cl_ord_id);
    assert(!std::empty(real_cl_ord_id));
    log::warn(R"(DEBUG: REMOVE {{client_id="{}", cl_ord_id(client)="{}"}})"sv, client_id, real_cl_ord_id);
    tmp.erase(real_cl_ord_id);
    if (std::empty(tmp))
      cl_ord_id_.lookup.erase(iter_2);
  }
}

std::string_view Controller::find_server_cl_ord_id(std::string_view const &cl_ord_id, std::string_view &client_id) {
  auto iter_1 = cl_ord_id_.lookup.find(client_id);
  if (iter_1 == std::end(cl_ord_id_.lookup))
    return {};
  auto &tmp = (*iter_1).second;
  auto iter_2 = tmp.find(cl_ord_id);
  if (iter_2 == std::end(tmp))
    return {};
  return (*iter_2).second;
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
