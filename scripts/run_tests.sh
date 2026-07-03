#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/test_market_data_parser
./build/test_feed_replay
./build/test_exchange_simulator
./build/test_order_gateway
./build/llgw

echo "All Day 4 checks passed."
