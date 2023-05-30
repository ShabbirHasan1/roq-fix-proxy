#!/usr/bin/env bash
./fix-bridge-client-simple \
  --config_file config/test.toml \
  --listen_port 1234 \
  tcp://localhost:2345
