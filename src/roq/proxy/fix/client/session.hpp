/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include "roq/event.hpp"
#include "roq/stop.hpp"
#include "roq/timer.hpp"
#include "roq/trace.hpp"

// inbound
#include "roq/fix/codec/business_message_reject.hpp"
#include "roq/fix/codec/execution_report.hpp"
#include "roq/fix/codec/market_data_incremental_refresh.hpp"
#include "roq/fix/codec/market_data_request_reject.hpp"
#include "roq/fix/codec/market_data_snapshot_full_refresh.hpp"
#include "roq/fix/codec/order_cancel_reject.hpp"

// outbound
#include "roq/fix/codec/market_data_request.hpp"
#include "roq/fix/codec/new_order_single.hpp"
#include "roq/fix/codec/order_cancel_replace_request.hpp"
#include "roq/fix/codec/order_cancel_request.hpp"
#include "roq/fix/codec/order_mass_cancel_request.hpp"
#include "roq/fix/codec/order_mass_status_request.hpp"
#include "roq/fix/codec/order_status_request.hpp"

namespace roq {
namespace proxy {
namespace fix {
namespace client {

struct Session {
  struct Handler {
    virtual void operator()(Trace<roq::fix::codec::OrderStatusRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<roq::fix::codec::MarketDataRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<roq::fix::codec::NewOrderSingle> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<roq::fix::codec::OrderCancelReplaceRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<roq::fix::codec::OrderCancelRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<roq::fix::codec::OrderMassStatusRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<roq::fix::codec::OrderMassCancelRequest> const &, std::string_view const &username) = 0;
  };

  virtual ~Session() = default;

  virtual void operator()(Event<Stop> const &) = 0;
  virtual void operator()(Event<Timer> const &) = 0;

  virtual void operator()(Trace<roq::fix::codec::BusinessMessageReject> const &) = 0;
  virtual void operator()(Trace<roq::fix::codec::MarketDataRequestReject> const &) = 0;
  virtual void operator()(Trace<roq::fix::codec::MarketDataSnapshotFullRefresh> const &) = 0;
  virtual void operator()(Trace<roq::fix::codec::MarketDataIncrementalRefresh> const &) = 0;
  virtual void operator()(Trace<roq::fix::codec::OrderCancelReject> const &) = 0;
  virtual void operator()(Trace<roq::fix::codec::ExecutionReport> const &) = 0;
};

}  // namespace client
}  // namespace fix
}  // namespace proxy
}  // namespace roq
