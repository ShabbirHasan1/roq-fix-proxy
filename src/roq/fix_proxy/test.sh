#!/usr/bin/env bash

./roq-fix-proxy \
  --config_file config/test.toml \
  --server_target_comp_id "roq-fix-bridge" \
  --server_sender_comp_id "roq-fix-client-test" \
  --server_username "trader" \
  --client_listen_address "tcp://localhost:1234" \
  --client_json_listen_address "tcp://localhost:2345" \
  tcp://localhost:3456 \
  $@
