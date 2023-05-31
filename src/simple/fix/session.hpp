/* Copyright (c) 2017-2023, Hans Erik Thrane */

#pragma once

#include <fmt/format.h>

#include <memory>
#include <vector>

#include "roq/event.hpp"
#include "roq/start.hpp"
#include "roq/stop.hpp"
#include "roq/timer.hpp"
#include "roq/trace.hpp"

#include "roq/io/context.hpp"

#include "roq/io/web/uri.hpp"

#include "roq/io/net/connection_factory.hpp"
#include "roq/io/net/connection_manager.hpp"

#include "roq/fix/message.hpp"

#include "roq/fix_bridge/fix/heartbeat.hpp"

#include "simple/settings.hpp"

namespace simple {
namespace fix {

// note! supports both rest and websocket

struct Session final : public roq::io::net::ConnectionManager::Handler {
  Session(Settings const &, roq::io::Context &, roq::io::web::URI const &);

  void operator()(roq::Event<roq::Start> const &);
  void operator()(roq::Event<roq::Stop> const &);
  void operator()(roq::Event<roq::Timer> const &);

 protected:
  // io::net::ConnectionManager::Handler
  void operator()(roq::io::net::ConnectionManager::Connected const &) override;
  void operator()(roq::io::net::ConnectionManager::Disconnected const &) override;
  void operator()(roq::io::net::ConnectionManager::Read const &) override;

  // inbound

  void check(roq::fix::Header const &);
  void parse(roq::Trace<roq::fix::Message> const &);

  void operator()(roq::Trace<roq::fix_bridge::fix::Heartbeat> const &, roq::fix::Header const &);

  // outbound

  void send_logon();

  template <typename T>
  void send(T const &event, std::chrono::nanoseconds sending_time);

 private:
  // config
  std::string_view const sender_comp_id_;
  std::string_view const target_comp_id_;
  bool const debug_;
  // connection
  std::unique_ptr<roq::io::net::ConnectionFactory> const connection_factory_;
  std::unique_ptr<roq::io::net::ConnectionManager> const connection_manager_;
  // messaging
  struct {
    uint64_t msg_seq_num = {};
  } inbound_;
  struct {
    uint64_t msg_seq_num = {};
  } outbound_;
  std::vector<std::byte> encode_buffer_;
};

}  // namespace fix
}  // namespace simple
