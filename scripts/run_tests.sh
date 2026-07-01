#!/usr/bin/env bash
set -euo pipefail

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/test_market_data_parser
./build/llgw

echo "All Day 2 checks passed."
