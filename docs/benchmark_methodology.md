# Benchmark Methodology

## Current Status

This project includes a parser benchmark smoke harness.

The harness exists to develop a documented and repeatable measurement process. Its output is not presented as production, exchange-grade, or hardware-independent latency.

## Why Batched Timing Is Used

Earlier versions timed every parser invocation with a separate pair of steady-clock calls.

That approach produced zero-nanosecond samples and inconsistent differences between input sets because timer resolution and clock-call overhead were significant relative to the measured operation.

The current harness groups multiple parser operations into each timed batch.

For every batch:

1. The clock is read.
2. Multiple parser operations are executed.
3. The clock is read again.
4. Elapsed time is divided by operations in that batch.

This reduces, but does not eliminate, timer overhead and quantization.

## Baseline Measurement

Each trial also measures a baseline loop.

The baseline performs input selection and checksum work without calling the parser.

Baseline results are reported alongside parser results, but they are not automatically subtracted.

Automatic subtraction is avoided because the baseline is not a perfect model of parser benchmark overhead. Subtracting noisy measurements could create misleading or negative estimates.

## Trial Ordering

Odd-numbered trials run:

baseline then parser

Even-numbered trials run:

parser then baseline

Alternating order reduces consistent warm-cache or run-order bias, but it does not eliminate environmental noise.

## Input Sets

The benchmark supports:

- single
- varied

The single input set repeatedly parses one valid line.

The varied input set cycles through eight valid lines with different symbols, prices, sizes, and spreads.

The varied set is broader than one fixed line but is not intended to model a realistic exchange-feed distribution.

## How To Run

Default run:

./scripts/run_benchmarks.sh

Configured batched run:

./scripts/run_benchmarks.sh --warmup 6400 --iterations 64000 --trials 3 --batch-size 64 --input-set varied

CSV output:

./scripts/run_benchmarks.sh --warmup 6400 --iterations 64000 --trials 3 --batch-size 64 --input-set varied --csv benchmark_results/parser_batched.csv

## Reported Information

The harness reports:

- warmup iterations
- measured parser operations
- trial count
- batch size
- timed batch count
- input set and input count
- compiler and compiler version
- C++ standard macro
- build mode
- operating-system macro
- architecture macro
- steady-clock property
- hardware concurrency
- clock-pair timing distribution
- baseline nanoseconds per operation
- parser nanoseconds per operation
- total baseline and parser elapsed time
- checksum

## Current Limitations

Known limitations include:

- normal developer-machine execution
- no CPU affinity
- no frequency-scaling control
- no process isolation
- no detailed CPU model
- no allocation instrumentation
- no hardware performance counters
- no formal benchmark framework
- no realistic feed distribution
- no malformed-input benchmark
- remaining clock and loop overhead
- operating-system scheduling noise
- nanosecond values derived from steady-clock duration conversion

## Reporting Rules

Do not describe these results as:

- ultra-low latency
- production-grade latency
- exchange-grade latency
- lock-free performance
- a comparison with professional trading systems

Acceptable descriptions include:

- implemented a batched local parser benchmark harness
- added repeated trials, input sets, metadata, CSV output, clock-pair reporting, and baseline measurements
- documented measurement limitations and avoided unsupported performance claims

## Future Improvements

Potential next steps include:

1. detailed CPU and OS-version metadata
2. benchmark runs under a formal framework
3. Linux CPU-affinity documentation
4. allocation instrumentation
5. realistic input distributions
6. malformed-input measurements
7. separate throughput-oriented timing
8. CI benchmark build-and-smoke verification without performance thresholds
