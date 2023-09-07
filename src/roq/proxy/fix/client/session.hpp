/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include "roq/event.hpp"
#include "roq/stop.hpp"
#include "roq/timer.hpp"
#include "roq/trace.hpp"

// inbound
#include "roq/codec/fix/business_message_reject.hpp"
#include "roq/codec/fix/execution_report.hpp"
#include "roq/codec/fix/market_data_incremental_refresh.hpp"
#include "roq/codec/fix/market_data_request_reject.hpp"
#include "roq/codec/fix/market_data_snapshot_full_refresh.hpp"
#include "roq/codec/fix/order_cancel_reject.hpp"
#include "roq/codec/fix/security_definition.hpp"
#include "roq/codec/fix/security_list.hpp"

// outbound
#include "roq/codec/fix/market_data_request.hpp"
#include "roq/codec/fix/new_order_single.hpp"
#include "roq/codec/fix/order_cancel_replace_request.hpp"
#include "roq/codec/fix/order_cancel_request.hpp"
#include "roq/codec/fix/order_mass_cancel_request.hpp"
#include "roq/codec/fix/order_mass_status_request.hpp"
#include "roq/codec/fix/order_status_request.hpp"
#include "roq/codec/fix/security_definition_request.hpp"
#include "roq/codec/fix/security_list_request.hpp"

namespace roq {
namespace proxy {
namespace fix {
namespace client {

struct Session {
  struct Handler {
    // market data
    virtual void operator()(Trace<codec::fix::SecurityListRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::SecurityDefinitionRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::MarketDataRequest> const &, std::string_view const &username) = 0;
    // order management
    virtual void operator()(Trace<codec::fix::OrderStatusRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::NewOrderSingle> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::OrderCancelReplaceRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::OrderCancelRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::OrderMassStatusRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<codec::fix::OrderMassCancelRequest> const &, std::string_view const &username) = 0;
  };

  virtual ~Session() = default;

  virtual void operator()(Event<Stop> const &) = 0;
  virtual void operator()(Event<Timer> const &) = 0;

  virtual void operator()(Trace<codec::fix::BusinessMessageReject> const &) = 0;
  // market data
  virtual void operator()(Trace<codec::fix::SecurityList> const &) = 0;
  virtual void operator()(Trace<codec::fix::SecurityDefinition> const &) = 0;
  virtual void operator()(Trace<codec::fix::MarketDataRequestReject> const &) = 0;
  virtual void operator()(Trace<codec::fix::MarketDataSnapshotFullRefresh> const &) = 0;
  virtual void operator()(Trace<codec::fix::MarketDataIncrementalRefresh> const &) = 0;
  // order management
  virtual void operator()(Trace<codec::fix::OrderCancelReject> const &) = 0;
  virtual void operator()(Trace<codec::fix::ExecutionReport> const &) = 0;
};

}  // namespace client
}  // namespace fix
}  // namespace proxy
}  // namespace roq
