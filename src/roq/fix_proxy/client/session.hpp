/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include "roq/stop.hpp"
#include "roq/timer.hpp"
#include "roq/trace.hpp"

#include "roq/fix_bridge/fix/business_message_reject.hpp"
#include "roq/fix_bridge/fix/execution_report.hpp"
#include "roq/fix_bridge/fix/new_order_single.hpp"
#include "roq/fix_bridge/fix/order_cancel_reject.hpp"
#include "roq/fix_bridge/fix/order_cancel_replace_request.hpp"
#include "roq/fix_bridge/fix/order_cancel_request.hpp"
#include "roq/fix_bridge/fix/order_mass_cancel_request.hpp"
#include "roq/fix_bridge/fix/order_mass_status_request.hpp"
#include "roq/fix_bridge/fix/order_status_request.hpp"

namespace roq {
namespace fix_proxy {
namespace client {

struct Session {
  struct Handler {
    virtual void operator()(Trace<fix_bridge::fix::OrderStatusRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<fix_bridge::fix::NewOrderSingle> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<fix_bridge::fix::OrderCancelReplaceRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(Trace<fix_bridge::fix::OrderCancelRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<fix_bridge::fix::OrderMassStatusRequest> const &, std::string_view const &username) = 0;
    virtual void operator()(
        Trace<fix_bridge::fix::OrderMassCancelRequest> const &, std::string_view const &username) = 0;
  };

  virtual ~Session() = default;

  virtual void operator()(Event<Stop> const &) = 0;
  virtual void operator()(Event<Timer> const &) = 0;

  virtual void operator()(Trace<fix_bridge::fix::BusinessMessageReject> const &) = 0;
  virtual void operator()(Trace<fix_bridge::fix::OrderCancelReject> const &) = 0;
  virtual void operator()(Trace<fix_bridge::fix::ExecutionReport> const &) = 0;
};

}  // namespace client
}  // namespace fix_proxy
}  // namespace roq
