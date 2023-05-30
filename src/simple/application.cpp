/* Copyright (c) 2017-2023, Hans Erik Thrane */

#include "simple/application.hpp"

#include <vector>

#include "roq/exceptions.hpp"

#include "roq/io/engine/libevent/context_factory.hpp"

#include "simple/config.hpp"
#include "simple/controller.hpp"

using namespace std::literals;

namespace simple {

// === IMPLEMENTATION ===

int Application::main_helper(std::span<std::string_view> const &args) {
  assert(!std::empty(args));
  Config config;
  // note!
  //   absl::flags will have removed all flags and we're left with arguments
  //   the arguments should be a list of unix domain sockets
  auto connections = args.subspan(1);  // first argument is the program name
  // note!
  //   client::Bridge allows us to
  //   * can dispatch events through the Timer event
  //   * connect directly to the gateways (if we later should decide to do that)
  //   we will use an explicit IO context so we can implement a networked solution
  auto context = roq::io::engine::libevent::ContextFactory::create();
  roq::client::Bridge{config, connections}.dispatch<Controller>(*context);
  return EXIT_SUCCESS;
}

int Application::main(int argc, char **argv) {
  // wrap arguments (prefer to not work with raw pointers)
  std::vector<std::string_view> args;
  args.reserve(argc);
  for (int i = 0; i < argc; ++i)
    args.emplace_back(argv[i]);
  return main_helper(args);
}

}  // namespace simple
