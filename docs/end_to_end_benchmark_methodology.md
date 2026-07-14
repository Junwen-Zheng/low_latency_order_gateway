# In-Memory End-to-End Benchmark Methodology

## Scope

This benchmark measures the deterministic in-memory path:

text market-data line
→ market-data parser
→ simple strategy
→ order pipeline
→ order gateway
→ exchange simulator

It intentionally excludes:

- feed-file opening
- disk I/O
- line reading
- FeedReplay callback dispatch
- networking
- operating-system socket handling
- live exchange connectivity

It should therefore not be described as complete application or live-feed throughput.

## Workloads

The benchmark supports two workloads.

### mixed

The mixed workload cycles through eight valid market-data messages.

Four messages generate orders and four messages do not.

This measures a path with both strategy action and no-action outcomes.

### all-orders

The all-orders workload uses eight valid messages where every message generates an order.

This increases work in:

- fixed-symbol copying
- pipeline enqueueing
- pipeline draining
- gateway submission
- exchange validation
- response counter updates

## Batch Behavior

Events are processed in configurable batches.

For every measured batch:

1. Text lines are parsed.
2. Parsed updates are passed to the strategy.
3. Generated orders are enqueued.
4. The pipeline is drained after all events in the batch.
5. Elapsed batch time is divided by events in the batch.

The configured batch size must not exceed pipeline capacity.

## Counter Validation

Each measured trial verifies:

- every event reached the strategy
- generated orders equal enqueued plus dropped orders
- no orders were dropped
- every enqueued order was drained
- every drained order was accepted
- gateway and pipeline counters agree
- no exchange rejection occurred

The benchmark exits with an error if these invariants fail.

## Stabilization and First-Trial Handling

The benchmark performs an untimed stabilization pass before measured trials.

The first measured trial remains visible and is labeled:

first_measured

Later trials are labeled:

steady_state_candidate

Two descriptive summaries are printed:

- all-trial summary
- first-trial-excluded steady-state candidate summary

The first trial is never silently discarded.

## Baseline Measurement

A separate baseline loop measures input selection and checksum work without parsing or executing the system path.

The baseline is reported but not automatically subtracted.

It is not a complete model of benchmark overhead, so automatic subtraction could produce misleading estimates.

## Throughput Field

timed_events_per_second is calculated from:

measured events / summed timed path-batch duration

It is not:

- file-replay throughput
- network throughput
- order throughput
- production throughput
- exchange throughput

orders_per_event is reported separately because the mixed and all-orders workloads perform different amounts of downstream work.

## How To Run

Mixed workload:

./scripts/run_end_to_end_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload mixed

All-orders workload:

./scripts/run_end_to_end_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload all-orders

CSV output:

./scripts/run_end_to_end_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload mixed --csv benchmark_results/end_to_end_mixed.csv

## Reporting Rules

Do not describe the benchmark as:

- exchange-grade
- production-grade
- end-to-end live trading latency
- network latency
- file-replay throughput
- lock-free performance

Acceptable wording:

- added an in-memory benchmark for the deterministic parser-to-exchange-simulator path
- measured mixed and all-order workloads separately
- added invariant validation, repeated trials, stabilization, baseline measurements, and CSV output
- documented that file I/O, feed replay, networking, and live exchange behavior are excluded

## Workload Comparison Follow-Up

The separate paired path-comparison benchmark was added to investigate workload comparability and the counterintuitive result that an all-orders workload could appear faster than a mixed workload.

See:

docs/path_comparison_benchmark_methodology.md

No causal conclusion should be drawn from the original mixed-versus-all-orders timing difference without reviewing the paired text and prepared-path measurements.

## Consolidated Suite

The in-memory end-to-end benchmark is included in docs/benchmark_suite.md.

File I/O, FeedReplay, networking, and live exchange behavior remain excluded.
