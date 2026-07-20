#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/test_fixed_symbol
./build/test_market_data_parser
./build/test_feed_replay
./build/test_exchange_simulator
./build/test_order_gateway
./build/test_simple_strategy
./build/test_end_to_end_order_flow
./build/test_ring_buffer
./build/test_order_pipeline
./build/test_end_to_end_pipeline
./build/llgw

./build/test_pre_trade_risk
./build/test_end_to_end_risk
./build/test_order_lifecycle
./build/test_end_to_end_lifecycle
./build/test_order_actions
./build/test_end_to_end_order_actions
./build/test_execution_report
./build/test_end_to_end_execution_report
echo "All repository checks passed."
