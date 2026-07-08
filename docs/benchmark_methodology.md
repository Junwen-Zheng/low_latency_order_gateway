# Benchmark Methodology

## Current Status

This project now includes a basic parser benchmark smoke harness.

The benchmark is intentionally early-stage. It is useful for checking that a measurement path exists, but the results should not be treated as a stable performance claim.

## Current Benchmark

The current benchmark executable is:

bench_parser

It repeatedly parses a fixed-format market-data line:

AAPL,192.10,100,192.12,200,1710000000123456789

The benchmark reports:

- warmup iterations
- measured iterations
- total elapsed nanoseconds
- min latency
- p50 latency
- p95 latency
- p99 latency
- max latency
- checksum

The checksum exists to make sure parsed results are consumed and the parser call is not trivially optimized away.

## How To Run

Run:

./scripts/run_benchmarks.sh

## Current Limitations

The current benchmark is not enough to claim production-grade latency.

Known limitations:

- Runs on a normal developer machine
- Uses std::chrono::steady_clock
- Measures one fixed parser input
- Does not pin CPU cores
- Does not control CPU frequency scaling
- Does not isolate the process
- Does not measure allocations
- Does not compare multiple parser implementations
- Does not test realistic feed variety
- Does not report compiler version or CPU model automatically
- Does not run repeated trials across clean system states

## Rules For Reporting Results

Do not write claims such as:

- ultra-low latency
- production-grade
- exchange-grade
- lock-free performance
- beats professional systems

Acceptable wording:

- Added a local parser benchmark smoke harness.
- Recorded local p50/p95/p99 parser timings under a documented, limited setup.
- Results are machine-dependent and not presented as production latency claims.

## Next Methodology Improvements

Before using benchmark numbers in a resume or README, add:

1. machine and compiler metadata
2. repeated runs
3. CSV output
4. benchmark configuration parameters
5. comparison between parser variants
6. explicit warmup and measurement notes
7. allocation measurement or instrumentation
8. CPU pinning notes, if running on Linux
