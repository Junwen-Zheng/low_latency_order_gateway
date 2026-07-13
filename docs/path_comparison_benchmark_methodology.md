# Paired Path-Comparison Benchmark Methodology

## Purpose

Day 15 showed that the all-orders workload could appear faster than the mixed workload.

That result must not be interpreted as evidence that additional downstream work is inherently faster.

This benchmark investigates workload comparability by measuring two paths within each trial.

## Compared Paths

### Text path

text market-data line
→ parser
→ strategy
→ order pipeline
→ gateway
→ exchange simulator

### Prepared path

pre-parsed market-data update
→ strategy
→ order pipeline
→ gateway
→ exchange simulator

The prepared inputs are created before stabilization and timing.

The paired difference is descriptive. It is not treated as an exact parser-cost measurement because the paths can differ in cache state, code layout, branch behavior, and compiler optimization.

## Controlled Workloads

The benchmark supports three workloads.

### no-orders

Every event reaches the strategy, but no order is generated.

Expected orders per event:

0.0

### mixed

Half the events generate orders and half do not.

Expected orders per event:

0.5

### all-orders

Every event generates an order.

Expected orders per event:

1.0

The same symbols, sizes, timestamps, and similar-length price fields are used across workloads. The main intended difference is whether the configured spread generates an order.

## Paired Trial Design

Both paths are measured in every trial.

Odd-numbered trials run:

text then prepared

Even-numbered trials run:

prepared then text

This reduces consistent path-order bias but does not eliminate environmental noise.

Each measured path uses a fresh system state.

Its warmup runs on the same state before timed batches, so exchange and pipeline data structures have already been exercised before measurement.

## Validation

For each path and workload, the benchmark validates:

- every measured event reaches the strategy
- generated-order count matches the workload
- no orders are dropped
- every enqueued order is drained
- every drained order is accepted
- pipeline and gateway rejection counts remain zero

The benchmark terminates if any invariant fails.

## Reported Differences

The benchmark reports:

text_minus_prepared_p50_ns_per_event

and:

text_minus_prepared_total_avg_ns_per_event

These are paired descriptive differences.

They must not be described as exact parser latency because the prepared path is a control path, not a perfect subtraction baseline.

## Scope Exclusions

The benchmark excludes:

- file I/O
- FeedReplay
- networking
- live exchange connectivity
- operating-system socket handling
- cross-thread queueing
- production workload distributions

## How To Run

No-orders workload:

./scripts/run_path_comparison_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload no-orders

Mixed workload:

./scripts/run_path_comparison_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload mixed

All-orders workload:

./scripts/run_path_comparison_benchmark.sh --stabilization-iterations 100000 --warmup 6400 --iterations 64000 --trials 5 --batch-size 64 --workload all-orders

## Reporting Rules

Do not claim that:

- more downstream work is inherently faster
- the paired delta is exact parser latency
- the benchmark represents production trading latency
- the benchmark represents live-feed throughput

Acceptable descriptions include:

- added a paired text-versus-prepared path benchmark
- compared no-order, mixed, and all-order workloads
- alternated path order across trials
- validated all strategy, pipeline, gateway, and exchange counters
- treated paired differences as descriptive rather than exact component attribution
