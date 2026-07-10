# Benchmark Methodology

## Current Status

This project includes a parser benchmark smoke harness.

The benchmark is useful for checking that a repeatable measurement path exists, but the results should not be treated as stable production latency claims.

## Benchmark Executable

The benchmark executable is:

bench_parser

It supports two input sets:

- single
- varied

The single input set repeatedly parses one fixed-format market-data line.

The varied input set cycles through multiple valid market-data lines with different symbols, prices, sizes, and spreads.

## How To Run

Default run:

./scripts/run_benchmarks.sh

Configured single-input run:

./scripts/run_benchmarks.sh --warmup 10000 --iterations 100000 --trials 5 --input-set single

Configured varied-input run:

./scripts/run_benchmarks.sh --warmup 10000 --iterations 100000 --trials 5 --input-set varied

CSV output:

./scripts/run_benchmarks.sh --warmup 10000 --iterations 100000 --trials 5 --input-set varied --csv benchmark_results/parser_varied.csv

## Reported Fields

The benchmark reports:

- warmup iterations
- measured iterations
- trial count
- input set
- input count
- compiler name
- compiler version
- C++ standard macro
- build mode
- operating system macro
- architecture macro
- whether std::chrono::steady_clock is steady
- hardware concurrency
- total elapsed nanoseconds per trial
- min latency
- p50 latency
- p95 latency
- p99 latency
- max latency
- checksum

The checksum exists to make sure parsed results are consumed and the parser call is not trivially optimized away.

## Current Limitations

The current benchmark is not enough to claim production-grade latency.

Known limitations:

- Runs on a normal developer machine
- Uses std::chrono::steady_clock
- Measures a small set of valid parser inputs
- Does not pin CPU cores
- Does not control CPU frequency scaling
- Does not isolate the process
- Does not measure allocations
- Does not compare multiple parser implementations
- Does not test malformed input performance
- Does not test realistic exchange feed distributions
- Does not automatically record detailed CPU model
- Does not run under a formal benchmarking framework
- Per-iteration timing includes measurement overhead

## Rules For Reporting Results

Do not write claims such as:

- ultra-low latency
- production-grade
- exchange-grade
- lock-free performance
- beats professional systems

Acceptable wording:

- Added a local parser benchmark smoke harness.
- Added configurable warmup, iteration count, repeated trials, CSV output, metadata, and input-set selection.
- Recorded local p50/p95/p99 parser timings under a documented, limited setup.
- Results are machine-dependent and not presented as production latency claims.

## Next Methodology Improvements

Before using benchmark numbers in a resume or README, add:

1. detailed machine and OS metadata
2. repeated benchmark result files across clean system states
3. benchmark input distributions closer to real feeds
4. comparison between parser variants
5. allocation measurement or instrumentation
6. CPU pinning notes, if running on Linux
7. clearer separation between benchmark timing overhead and parser cost
8. CI smoke run for benchmark buildability, not performance gates
