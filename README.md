# Low-Latency C++ Order Gateway Simulator

A deterministic C++20 execution-systems lab that models the path from market-data parsing to exchange acknowledgement.

The repository is designed as a portfolio and research artifact for latency-sensitive trading infrastructure. It emphasizes correctness, explicit state transitions, ownership discipline, reproducible benchmarks, and honest scope boundaries.

## System Flow

```text
sample feed
    ↓
feed replay
    ↓
market-data parser
    ↓
simple strategy
    ↓
fixed-capacity order pipeline
    ↓
pre-trade risk
    ↓
order gateway
    ↓
exchange simulator
    ↓
lifecycle tracking
    ↓
structured execution reports
```

Accepted orders can also be amended or cancelled:

```text
exchange_accepted → amend_pending → exchange_accepted
exchange_accepted → cancel_pending → cancelled
```

## Capabilities

- fixed-format market-data parsing with explicit error statuses
- deterministic file replay
- fixed-capacity ring buffer and order pipeline
- owned fixed-size symbols for queued orders
- pre-trade quantity, notional, and symbol-allowlist checks
- deterministic exchange acceptance and rejection
- active-order storage
- order amendment and cancellation
- enforced lifecycle transitions
- structured execution reports with stable rejection fields
- unit, integration, end-to-end, and hardening tests
- CTest registration
- warnings-as-errors builds
- platform-aware sanitizer checks
- reproducible local benchmark harnesses
- GitHub Actions correctness and benchmark smoke checks

## Build and Run

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/llgw
```

## Tests

```bash
./scripts/run_tests.sh
```

Or through CTest:

```bash
ctest --test-dir build --output-on-failure
```

## Quality Gate

```bash
./scripts/run_quality_gate.sh
```

The quality gate runs:

- a clean Release build with warnings treated as errors
- all 20 registered CTest cases
- a clean sanitizer build
- all 20 CTest cases under platform-supported sanitizers

Sanitizer policy:

- AppleClang on macOS: UndefinedBehaviorSanitizer
- Clang or GCC on Linux: AddressSanitizer and UndefinedBehaviorSanitizer

## Final Validation

```bash
./scripts/run_final_validation.sh
```

This runs the quality gate, the seven-case quick benchmark suite, repository consistency checks, and release-document checks.

## Benchmarks

Quick suite:

```bash
./scripts/run_benchmark_suite.sh
```

Larger local suite:

```bash
./scripts/run_benchmark_suite.sh \
  --mode full \
  --prefix benchmark_results/local_full_suite
```

The suite covers:

- parser: single and varied input sets
- in-memory parser-to-exchange path: mixed and all-orders workloads
- paired text versus prepared path: no-orders, mixed, and all-orders workloads

Results are local and machine-dependent. They exclude networking, file I/O in timed paths, live exchange behavior, cross-thread communication, and production workload distributions. Benchmark values are not CI performance thresholds.

See:

- `docs/benchmark_methodology.md`
- `docs/end_to_end_benchmark_methodology.md`
- `docs/path_comparison_benchmark_methodology.md`
- `docs/benchmark_suite.md`
- `docs/final_report.md`

## Repository Structure

```text
include/llgw/       public C++ headers
src/                implementations and demo
tests/              correctness and hardening tests
benchmarks/         benchmark executables
scripts/            build, test, benchmark, CI, and validation helpers
data/               deterministic sample feed
docs/               architecture, methodology, audit, and release documents
.github/workflows/  CI configuration
```

## Design Principles

- correctness before optimization
- deterministic behavior before concurrency
- explicit rejection reasons and lifecycle states
- bounded storage where it matters to the modeled path
- no hidden benchmark subtraction
- no unsupported latency claims
- documented ownership and failure boundaries
- reproducible validation over anecdotal success

## Scope Boundaries

This project is not a live or production trading gateway. It does not provide:

- network transport or exchange protocols
- multithreaded or lock-free execution
- persistent state or crash recovery
- fills, partial fills, or a matching engine
- exchange-assigned order identifiers
- clock synchronization
- durable audit storage
- production risk governance
- authentication or secrets handling
- a real alpha strategy

## Documentation

- `docs/architecture.md` — current component and data-flow design
- `docs/project_status.md` — completed scope and release status
- `docs/production_readiness.md` — hardening evidence and production boundary
- `docs/final_report.md` — final technical report
- `docs/portfolio_summary.md` — concise project presentation
- `docs/release_checklist.md` — release procedure
- `docs/study_log.md` — day-by-day implementation record
