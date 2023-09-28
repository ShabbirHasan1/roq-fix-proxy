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

// === IMPLEMENTATION ===

Controller::Controller(
    Settings const &settings,
    Config const &config,
    io::Context &context,
    std::span<std::string_view const> const &connections)
    : context_{context}, terminate_{context.create_signal(*this, io::sys::Signal::Type::TERMINATE)},
      interrupt_{context.create_signal(*this, io::sys::Signal::Type::INTERRUPT)},
      timer_{context.create_timer(*this, TIMER_FREQUENCY)}, shared_{settings, config},
      server_manager_{*this, settings, context, shared_, connections},
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

void Controller::operator()(Trace<server::Session::Ready> const &, std::string_view const &username) {
  ready_ = true;
}

void Controller::operator()(Trace<server::Session::Disconnected> const &, std::string_view const &username) {
  ready_ = false;
  client_manager_.get_all_sessions([&](auto &session) { session.force_disconnect(); });
}

void Controller::operator()(Trace<codec::fix::BusinessMessageReject> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(
    Trace<codec::fix::UserResponse> const &event, [[maybe_unused]] std::string_view const &username) {
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

void Controller::operator()(Trace<codec::fix::SecurityList> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::SecurityDefinition> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::SecurityStatus> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::MarketDataRequestReject> const &event, std::string_view const &username) {
  auto &trace_info = event.trace_info;
  auto &market_data_reject = event.value;
  find_server_subscription(market_data_reject.md_req_id, [&]([[maybe_unused]] auto session_id, auto &client_md_req_id) {
    auto market_data_reject_2 = market_data_reject;
    market_data_reject_2.md_req_id = client_md_req_id;
    Trace event_2{trace_info, market_data_reject_2};
    dispatch_to_client(event_2, username);
  });
}

void Controller::operator()(
    Trace<codec::fix::MarketDataSnapshotFullRefresh> const &event, std::string_view const &username) {
  auto &trace_info = event.trace_info;
  auto &market_data_snapshot_full_refresh = event.value;
  find_server_subscription(
      market_data_snapshot_full_refresh.md_req_id, [&]([[maybe_unused]] auto session_id, auto &client_md_req_id) {
        auto market_data_snapshot_full_refresh_2 = market_data_snapshot_full_refresh;
        market_data_snapshot_full_refresh_2.md_req_id = client_md_req_id;
        Trace event_2{trace_info, market_data_snapshot_full_refresh_2};
        dispatch_to_client(event_2, username);
      });
}

void Controller::operator()(
    Trace<codec::fix::MarketDataIncrementalRefresh> const &event, std::string_view const &username) {
  auto &trace_info = event.trace_info;
  auto &market_data_incremental_refresh = event.value;
  find_server_subscription(
      market_data_incremental_refresh.md_req_id, [&]([[maybe_unused]] auto session_id, auto &client_md_req_id) {
        auto market_data_incremental_refresh_2 = market_data_incremental_refresh;
        market_data_incremental_refresh_2.md_req_id = client_md_req_id;
        Trace event_2{trace_info, market_data_incremental_refresh_2};
        dispatch_to_client(event_2, username);
      });
}

void Controller::operator()(Trace<codec::fix::OrderCancelReject> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::ExecutionReport> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::RequestForPositionsAck> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

void Controller::operator()(Trace<codec::fix::PositionReport> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
}

// client::Session::Handler

void Controller::operator()(Trace<client::Session::Disconnected> const &event, std::string_view const &username) {
  auto &[trace_info, disconnect] = event;
  // market data
  auto iter_1 = subscriptions_.market_data.client_to_server.find(disconnect.session_id);
  if (iter_1 != std::end(subscriptions_.market_data.client_to_server)) {
    for (auto &[_, server_md_req_id] : (*iter_1).second) {
      if (ready()) {
        auto market_data_request = codec::fix::MarketDataRequest{
            .md_req_id = server_md_req_id,
            .subscription_request_type = roq::fix::SubscriptionRequestType::UNSUBSCRIBE,
            .market_depth = {},
            .md_update_type = {},
            .aggregated_book = {},
            .no_md_entry_types = {},
            .no_related_sym = {},
            .no_trading_sessions = {},
            .custom_type = {},
            .custom_value = {},
        };
        Trace event_2{trace_info, market_data_request};
        dispatch_to_server(event_2, username);
      }
      subscriptions_.market_data.server_to_client.erase(server_md_req_id);
    }
    subscriptions_.market_data.client_to_server.erase(iter_1);
  } else {
    log::debug("no market data subscriptions associated with session_id={}"sv, disconnect.session_id);
  }
  // user
  auto iter_2 = subscriptions_.user.session_to_username.find(disconnect.session_id);
  if (iter_2 != std::end(subscriptions_.user.session_to_username)) {
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
      Trace event_2{trace_info, user_request};
      dispatch_to_server(event_2, username);
      subscriptions_.user.server_to_client.try_emplace(user_request.user_request_id, disconnect.session_id);
      subscriptions_.user.client_to_server.try_emplace(disconnect.session_id, user_request.user_request_id);
    }
    // note! there are two scenarios
    // we can't send ==> fix-bridge is disconnected so it doesn't matter
    // we get a response => fix-bridge was connect and we expect it to do the right thing
    // therefore: release immediately to allow the client to reconnect
    log::debug(R"(USER REMOVE username="{}" <==> session_id={})"sv, username_2, disconnect.session_id);
    subscriptions_.user.username_to_session.erase((*iter_2).second);
    subscriptions_.user.session_to_username.erase(iter_2);
  } else {
    log::debug("no user associated with session_id={}"sv, disconnect.session_id);
  }
}

void Controller::operator()(
    Trace<codec::fix::UserRequest> const &event, std::string_view const &username, uint64_t session_id) {
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
    dispatch_to_server(event, username);
  } else {
    log::fatal("Unexpected"sv);
  }
}

void Controller::operator()(Trace<codec::fix::SecurityListRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(
    Trace<codec::fix::SecurityDefinitionRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<codec::fix::SecurityStatusRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(
    Trace<codec::fix::MarketDataRequest> const &event, std::string_view const &username, uint64_t session_id) {
  auto &[trace_info, market_data_request] = event;
  auto &tmp = subscriptions_.market_data.client_to_server[session_id];
  auto iter = tmp.find(market_data_request.md_req_id);
  if (iter == std::end(tmp)) {
    auto request_id = shared_.create_request_id();
    auto market_data_request_2 = market_data_request;
    market_data_request_2.md_req_id = request_id;
    Trace event_2{trace_info, market_data_request_2};
    dispatch_to_server(event_2, username);
    subscriptions_.market_data.client_to_server[session_id][market_data_request.md_req_id] =
        market_data_request_2.md_req_id;
    subscriptions_.market_data.server_to_client[market_data_request_2.md_req_id] =
        std::make_pair(session_id, market_data_request.md_req_id);
  } else {
    // XXX TODO reject
  }
}

void Controller::operator()(Trace<codec::fix::OrderStatusRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<codec::fix::NewOrderSingle> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(
    Trace<codec::fix::OrderCancelReplaceRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<codec::fix::OrderCancelRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<codec::fix::OrderMassStatusRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<codec::fix::OrderMassCancelRequest> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

void Controller::operator()(Trace<codec::fix::RequestForPositions> const &event, std::string_view const &username) {
  dispatch_to_server(event, username);
}

// utilities

template <typename... Args>
void Controller::dispatch(Args &&...args) {
  auto message_info = MessageInfo{};
  Event event{message_info, std::forward<Args>(args)...};
  server_manager_(event);
  client_manager_(event);
}

template <typename T>
void Controller::dispatch_to_server(Trace<T> const &event, std::string_view const &username) {
  if (server_manager_.find(username, [&](auto &session) { session(event); })) {
  } else {
    // log::fatal(R"(Unexpected: username="{}")"sv, username);  // note! should not be possible
    log::warn(R"(Unexpected: username="{}")"sv, username);  // note! should not be possible
  }
}

template <typename T>
void Controller::dispatch_to_client(Trace<T> const &event, std::string_view const &username) {
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

template <typename Callback>
bool Controller::find_server_subscription(std::string_view const &md_req_id, Callback callback) {
  auto iter = subscriptions_.market_data.server_to_client.find(md_req_id);
  if (iter == std::end(subscriptions_.market_data.server_to_client))
    return false;
  auto &[session_id, client_md_req_id] = (*iter).second;
  callback(session_id, client_md_req_id);
  return true;
}

void Controller::user_add(std::string_view const &username, uint64_t session_id) {
  log::info(R"(DEBUG: USER ADD username="{}" <==> session_id={})"sv, username, session_id);
  auto res_1 = subscriptions_.user.username_to_session.try_emplace(username, session_id).second;
  if (!res_1)
    log::fatal("Unexpected"sv);
  auto res_2 = subscriptions_.user.session_to_username.try_emplace(session_id, username).second;
  if (!res_2)
    log::fatal("Unexpected"sv);
}

void Controller::user_remove(std::string_view const &username, bool ready) {
  auto iter = subscriptions_.user.username_to_session.find(username);
  if (iter != std::end(subscriptions_.user.username_to_session)) {
    auto session_id = (*iter).second;
    log::info(R"(DEBUG: USER REMOVE username="{}" <==> session_id={})"sv, username, session_id);
    subscriptions_.user.session_to_username.erase(session_id);
    subscriptions_.user.username_to_session.erase(iter);
  } else if (ready) {
    // note! disconnect doesn't wait before cleaning up the resources
    log::fatal(R"(Unexpected: username="{}")"sv, username);
  }
}

bool Controller::user_is_locked(std::string_view const &username) const {
  auto iter = subscriptions_.user.username_to_session.find(username);
  return iter != std::end(subscriptions_.user.username_to_session);
}

}  // namespace fix
}  // namespace proxy
}  // namespace roq
