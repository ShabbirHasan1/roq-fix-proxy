#!/usr/bin/env bash
./roq-fix-proxy \
  --config_file config/test.toml \
  --fix_target_comp_id "roq-fix-bridge" \
  --fix_sender_comp_id "roq-fix-client-test" \
  --fix_username "trader" \
  --rest_listen_address 2345 \
  tcp://localhost:3456 \
  $@
