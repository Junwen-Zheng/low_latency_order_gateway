# Low-Latency C++ Order Gateway Simulator

This is a C++20 learning project focused on deterministic systems components behind latency-sensitive trading infrastructure.

The project currently covers:

- fixed-format market-data parsing
- deterministic feed replay
- simple strategy-to-order flow
- order gateway simulation
- exchange-side accept/reject simulation
- fixed-size ring-buffer-backed order pipeline
- correctness-focused tests

This project is intentionally scoped as a simulator. It is not connected to a live exchange and does not implement a real trading strategy.

## Current System Flow

sample feed file
    ↓
feed replay
    ↓
market-data parser
    ↓
simple strategy
    ↓
order pipeline
    ↓
order gateway
    ↓
exchange simulator

## Build

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

## Run Demo

./build/llgw

Expected demo output includes replay, strategy, pipeline, and gateway summaries.

## Run Tests

./scripts/run_tests.sh

The test script currently runs parser, feed replay, exchange simulator, order gateway, strategy, end-to-end order flow, ring buffer, order pipeline, and end-to-end pipeline tests.

## Repository Structure

include/llgw/      public headers
src/               implementation files
tests/             correctness-focused test executables
data/              sample feed data
docs/              design notes, architecture notes, study log
scripts/           helper scripts

## Design Principles

- Correctness before performance claims
- Deterministic tests before optimization
- Explicit error/status handling
- Small components with clear responsibilities
- Honest documentation of limitations
- No alpha/profitability claims
- No production-readiness claims

## Documentation

- docs/architecture.md explains the current system design and limitations.
- docs/project_status.md summarizes completed work and near-term next steps.
- docs/study_log.md records the staged build process.
- docs/stage1_design_notes.md captures parser and replay design notes.

## Current Limitations

- No latency benchmarks yet
- No network input yet
- No async/threaded pipeline yet
- No order book, fills, partial fills, or matching engine
- No live exchange connectivity
- Queued order symbols currently require careful lifetime handling because symbols are still represented with std::string_view

## Next Step

The next architecture step is to replace queued order symbols with owned or fixed-size symbol storage before introducing asynchronous queueing or benchmarking.

## Benchmarks

The project includes a batched parser benchmark smoke harness.

Example single-input run:

./scripts/run_benchmarks.sh --warmup 6400 --iterations 64000 --trials 3 --batch-size 64 --input-set single

Example varied-input run:

./scripts/run_benchmarks.sh --warmup 6400 --iterations 64000 --trials 3 --batch-size 64 --input-set varied

Example CSV output:

./scripts/run_benchmarks.sh --warmup 6400 --iterations 64000 --trials 3 --batch-size 64 --input-set varied --csv benchmark_results/parser_batched.csv

The harness reports batched parser timing, a separate baseline loop, clock-pair timing, repeated trials, and environment metadata.

The baseline is not automatically subtracted because it is not a perfect model of benchmark overhead.

Results are local and machine-dependent and should not be interpreted as production latency claims. See docs/benchmark_methodology.md.
