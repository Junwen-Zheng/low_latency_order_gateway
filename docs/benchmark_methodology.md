# Benchmark Methodology

## Current Status

This project includes a parser benchmark smoke harness.

The harness exists to develop a documented and repeatable measurement process. Its output is not presented as production, exchange-grade, or hardware-independent latency.

## Process Stabilization

Before clock calibration and measured trials, the harness performs an untimed parser stabilization pass.

The stabilization pass:

- exercises the selected input set
- consumes parser results through a checksum
- gives the process, instruction path, and CPU state time to move away from initial cold-start conditions

The number of stabilization operations is configurable with:

--stabilization-iterations

This reduces first-run effects but does not guarantee a thermally or operationally stable system.

## Batched Timing

The harness groups multiple parser operations into each timed batch.

For every batch:

1. The clock is read.
2. Multiple parser operations are executed.
3. The clock is read again.
4. Elapsed time is divided by the number of operations in that batch.

This reduces, but does not eliminate, timer overhead and quantization.

## Baseline Measurement

Each trial also measures a baseline loop.

The baseline performs input selection and checksum work without calling the parser.

Baseline results are reported alongside parser results but are not automatically subtracted.

Automatic subtraction is avoided because the baseline is not a perfect model of parser benchmark overhead. Subtracting noisy measurements could create misleading or negative estimates.

## First-Trial Handling

The first measured trial is preserved in console output and CSV output.

It is labeled:

first_measured

Later trials are labeled:

steady_state_candidate

Two cross-trial summaries are reported:

- an all-trial descriptive summary
- a steady-state candidate summary that excludes only the first measured trial

The first trial is not deleted or hidden. The separate summary makes cold-start sensitivity visible while allowing later trials to be reviewed independently.

## Trial Ordering

Odd-numbered trials run:

baseline then parser

Even-numbered trials run:

parser then baseline

Alternating order reduces consistent run-order bias but does not eliminate environmental noise.

## Timed Throughput Field

Each trial reports parser_timed_ops_per_second.

This is calculated from measured parser operations divided by the sum of timed parser-batch durations.

It is not an end-to-end feed throughput result. It excludes setup, sorting, reporting, CSV writing, and time between measured batches.

## Input Sets

The benchmark supports:

- single
- varied

The single input set repeatedly parses one valid line.

The varied input set cycles through eight valid lines with different symbols, prices, sizes, and spreads.

The varied set is broader than one fixed line but is not intended to model a realistic exchange-feed distribution.

## How To Run

Single-input run:

./scripts/run_benchmarks.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --input-set single

Varied-input run:

./scripts/run_benchmarks.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --input-set varied

CSV output:

./scripts/run_benchmarks.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --input-set varied --csv benchmark_results/parser_stabilized.csv

## Reported Information

The harness reports:

- stabilization iterations and elapsed time
- warmup iterations
- measured parser operations
- trial count
- trial phase
- batch size and timed batch count
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
- timed parser operations per second
- all-trial summary
- steady-state candidate summary
- checksums

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
- stabilization does not guarantee thermal equilibrium
- timed throughput excludes non-measured benchmark work

## Reporting Rules

Do not describe these results as:

- ultra-low latency
- production-grade latency
- exchange-grade latency
- lock-free performance
- professional-system performance

Acceptable descriptions include:

- implemented a batched local parser benchmark harness
- added process stabilization, first-trial labeling, repeated trials, metadata, CSV output, clock-pair reporting, and baseline measurements
- reported separate all-trial and steady-state candidate summaries
- documented measurement limitations and avoided unsupported performance claims

## Future Improvements

Potential next steps include:

1. detailed CPU and OS-version metadata
2. Linux CPU-affinity documentation
3. allocation instrumentation
4. realistic input distributions
5. malformed-input measurements
6. separate end-to-end throughput measurements
7. CI benchmark build-and-smoke verification without performance thresholds
8. comparison against a deliberately simple reference parser
