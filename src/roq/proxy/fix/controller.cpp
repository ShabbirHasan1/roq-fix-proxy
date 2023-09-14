/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "roq/proxy/fix/controller.hpp"

#include "roq/event.hpp"
#include "roq/timer.hpp"

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

/*
void Controller::operator()(Trace<codec::fix::SecurityDefinition> const &event) {
  auto &[trace_info, security_definition] = event;
  shared_.symbols.emplace(security_definition.symbol);  // XXX TODO cache reference data
}
*/

void Controller::operator()(Trace<codec::fix::BusinessMessageReject> const &event, std::string_view const &username) {
  dispatch_to_client(event, username);
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
  auto &[trace_info, market_data_reject] = event;
  find_server_subscription(market_data_reject.md_req_id, [&]([[maybe_unused]] auto session_id, auto &client_md_req_id) {
    auto market_data_reject_2 = market_data_reject;
    market_data_reject_2.md_req_id = client_md_req_id;
    Trace event_2{trace_info, market_data_reject_2};
    dispatch_to_client(event_2, username);
  });
}

void Controller::operator()(
    Trace<codec::fix::MarketDataSnapshotFullRefresh> const &event, std::string_view const &username) {
  auto &[trace_info, market_data_snapshot_full_refresh] = event;
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
  auto &[trace_info, market_data_incremental_refresh] = event;
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

// client::Session::Handler

void Controller::operator()(Trace<client::Session::Disconnect> const &event, std::string_view const &username) {
  auto &[trace_info, disconnect] = event;
  auto iter = subscriptions_.session_to_server.find(disconnect.session_id);
  if (iter != std::end(subscriptions_.session_to_server)) {
    for (auto &[_, server_md_req_id] : (*iter).second) {
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
      subscriptions_.server_to_session.erase(server_md_req_id);
    }
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
  auto &tmp = subscriptions_.session_to_server[session_id];
  auto iter = tmp.find(market_data_request.md_req_id);
  if (iter == std::end(tmp)) {
    auto request_id = shared_.create_request_id();
    auto market_data_request_2 = market_data_request;
    market_data_request_2.md_req_id = request_id;
    Trace event_2{trace_info, market_data_request_2};
    dispatch_to_server(event_2, username);
    subscriptions_.session_to_server[session_id][market_data_request.md_req_id] = market_data_request_2.md_req_id;
    subscriptions_.server_to_session[market_data_request_2.md_req_id] =
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
    log::fatal(R"(Unexpected: username="{}")"sv, username);  // note! should not be possible
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
  auto iter = subscriptions_.server_to_session.find(md_req_id);
  if (iter == std::end(subscriptions_.server_to_session))
    return false;
  auto &[session_id, client_md_req_id] = (*iter).second;
  callback(session_id, client_md_req_id);
  return true;
}

}  // namespace fix
}  // namespace proxy
}  // namespace roq
