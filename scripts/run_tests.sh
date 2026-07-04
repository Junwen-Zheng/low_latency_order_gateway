#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/test_market_data_parser
./build/test_feed_replay
./build/test_exchange_simulator
./build/test_order_gateway
./build/test_simple_strategy
./build/test_end_to_end_order_flow
./build/llgw

echo "All Day 5 checks passed."
