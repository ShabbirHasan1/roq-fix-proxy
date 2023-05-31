#!/usr/bin/env bash
./fix-bridge-client-simple \
  --config_file config/test.toml \
  --listen_port 2345 \
  --fix_target_comp_id "roq-fix-bridge" \
  --fix_sender_comp_id "roq-fix-client-test" \
  --fix_username "tbom1" \
  tcp://localhost:3456
