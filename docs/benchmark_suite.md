# Benchmark Suite

## Purpose

This suite provides one entry point for the existing local benchmark harnesses.

It consolidates commands without turning benchmark values into production claims or performance gates.

## Commands

Quick smoke suite:

    ./scripts/run_benchmark_suite.sh

Larger local suite:

    ./scripts/run_benchmark_suite.sh --mode full --prefix benchmark_results/local_full_suite

Generated CSV files are local artifacts and are ignored by Git.

## Included Measurements

- Parser benchmark: single and varied inputs
- In-memory system path: mixed and all-orders workloads
- Paired text-versus-prepared path: no-orders, mixed, and all-orders workloads

## Scope Boundaries

The suite does not measure file I/O, FeedReplay, networking, sockets, live exchange connectivity, cross-thread communication, or production workload distributions.

Only compare results when the executable, workload, batch size, iteration counts, trial count, compiler, build mode, machine, and operating environment match.

The three benchmark executables measure different paths and their latency values are not interchangeable.

## Interpretation Rules

- Raw first-trial results remain visible.
- First-trial-excluded summaries do not prove thermal stability.
- Baseline values are not automatically subtracted.
- Text-versus-prepared differences are descriptive, not exact parser latency.

## Performance Gates

Benchmark values must not be CI pass-or-fail thresholds.

Automated checks may verify compilation, successful execution, counter invariants, and non-empty CSV output.

## Reporting Rules

Do not claim production-grade latency, exchange-grade latency, live-trading throughput, network latency, lock-free performance, or superiority to professional systems.

## Continuous Integration

The lightweight CI workflow runs the repository correctness checks and this suite in `quick` mode.

CI verifies successful execution and seven non-empty CSV outputs. It does not compare benchmark values against performance thresholds.

See `docs/ci.md`.
