# Low-Latency C++ Order Gateway Simulator

This is a C++20/Linux learning project focused on the systems components behind latency-sensitive trading infrastructure: market-data parsing, queueing, order gateway simulation, deterministic replay, and latency measurement.

This project is intentionally scoped as a simulator. It is not connected to a live exchange and does not implement a real trading strategy.

## Current Status

Day 1 foundation:

- Basic CMake project structure
- Fixed-format market-data message model
- Initial parser using std::string_view and std::from_chars
- Simple assert-based parser tests
- Minimal executable demonstrating parser behavior

## Message Format

The initial market-data line format is:

SYMBOL,BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE,EXCHANGE_TS_NS

Example:

AAPL,192.10,100,192.12,200,1710000000123456789

## Build

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

## Run

./build/llgw

## Test

./build/test_market_data_parser

Or:

./scripts/run_tests.sh

## Design Principles

- Keep the hot path simple and measurable.
- Prefer fixed-format parsing before adding generality.
- Avoid heap allocation in parser code paths where practical.
- Add correctness tests before claiming performance.
- Document limitations and benchmark methodology honestly.
