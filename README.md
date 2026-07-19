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

The project includes a batched parser benchmark smoke harness with process stabilization and explicit first-trial handling.

Example single-input run:

./scripts/run_benchmarks.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --input-set single

Example varied-input run:

./scripts/run_benchmarks.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --input-set varied

Example CSV output:

./scripts/run_benchmarks.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --input-set varied --csv benchmark_results/parser_stabilized.csv

The first measured trial remains visible but is excluded from a separate steady-state candidate summary. The harness also reports a separate baseline loop, clock-pair timing, timed parser throughput, repeated trials, and environment metadata.

Results are local and machine-dependent and should not be interpreted as production latency or throughput claims. See docs/benchmark_methodology.md.

### In-Memory End-to-End Path Benchmark

A separate benchmark measures:

text line → parser → strategy → order pipeline → gateway → exchange simulator

Mixed workload:

./scripts/run_end_to_end_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload mixed

All-orders workload:

./scripts/run_end_to_end_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload all-orders

The benchmark excludes file I/O, feed replay, networking, and live exchange behavior. It validates system counters and reports local timed events per second, not production throughput.

See docs/end_to_end_benchmark_methodology.md.

### Paired Path-Comparison Benchmark

A paired benchmark compares:

text line → parser → strategy → pipeline → gateway → exchange

against:

pre-parsed update → strategy → pipeline → gateway → exchange

It supports no-orders, mixed, and all-orders workloads.

Example:

./scripts/run_path_comparison_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload mixed

The paired difference is descriptive and must not be interpreted as exact parser latency.

See docs/path_comparison_benchmark_methodology.md.

### Consolidated Benchmark Suite

Run the quick local suite:

    ./scripts/run_benchmark_suite.sh

Run the larger local suite:

    ./scripts/run_benchmark_suite.sh --mode full --prefix benchmark_results/local_full_suite

The suite builds once and runs the parser, in-memory end-to-end, and paired path-comparison benchmarks.

CSV outputs are local artifacts, not CI performance gates or production claims.

See docs/benchmark_suite.md.

### Continuous Integration

GitHub Actions runs the repository checks and the quick seven-case benchmark smoke suite on pushes to `main`, pull requests, and manual dispatch.

Run the same checks locally:

    ./scripts/run_ci_smoke.sh benchmark_results/local_ci_smoke

CI validates successful builds, tests, benchmark execution, invariants, and CSV creation. It does not enforce latency or throughput thresholds.

See `docs/ci.md`.

### Pre-Trade Risk Layer

Orders can be checked before they reach the gateway:

```text
strategy
→ order pipeline
→ pre-trade risk manager
→ order gateway
→ exchange simulator
```

The deterministic checks cover symbol validity, finite positive price, positive quantity, maximum quantity, maximum notional, and an optional symbol allowlist.

Risk-rejected orders are counted by explicit reason and never reach the gateway.

See `docs/pre_trade_risk.md`.

### Order Lifecycle Tracking

Orders can be tracked through deterministic states:

```text
created → queued → risk_rejected
```

or:

```text
created → queued → sent → exchange_accepted | exchange_rejected
```

The tracker enforces legal transitions, preserves rejection reasons, rejects duplicate tracking, and verifies that risk-rejected orders never reach the gateway.

See `docs/order_lifecycle.md`.

### Cancellation and Amendment Flow

Accepted exchange orders can be amended or cancelled deterministically.

```text
exchange_accepted → amend_pending → exchange_accepted
exchange_accepted → cancel_pending → cancelled
```

Rejected amendments preserve the original active order. Illegal lifecycle actions are rejected before reaching the exchange.

See `docs/order_actions.md`.
