# Final Technical Report

## Executive Summary

This repository implements a deterministic C++20 order-gateway simulator covering market-data parsing, strategy-triggered order creation, bounded queueing, pre-trade risk, exchange simulation, lifecycle enforcement, order actions, and structured execution reporting.

The project was built in staged increments, with correctness and reproducibility added before performance measurement. The final repository includes a strict compiler gate, 20 CTest cases, platform-supported sanitizer validation, benchmark methodology, CI automation, and explicit production limitations.

## Implemented System

```text
feed replay
→ parser
→ strategy
→ fixed-capacity pipeline
→ pre-trade risk
→ gateway
→ exchange simulator
→ lifecycle tracker
→ execution-report journal
```

Accepted orders support deterministic amendment and cancellation.

## Correctness Evidence

The final quality gate validates:

- Release build with warnings as errors
- 20 registered CTest cases
- hardening edge cases
- sanitizer build
- 20 CTest cases under sanitizers

Hardening coverage includes NaN and infinite prices, invalid amendments preserving original state, cancelled sequence IDs remaining non-reusable, bounded allowlist handling, null exchange behavior, null gateway behavior, lifecycle rollback, and unknown report queries.

## Benchmark Design

Three benchmark executables are provided:

1. parser benchmark
2. in-memory parser-to-exchange benchmark
3. paired text-versus-prepared path benchmark

Methodology includes process stabilization, batched timing, repeated trials, first-trial visibility, separate steady-state-candidate summaries, baseline reporting without automatic subtraction, alternating path or baseline order, counter and checksum validation, CSV output, and environment metadata.

## Day 17 Quick-Suite Snapshot

The following values come from the stored Day 17 quick-suite CSV artifacts. They are local, machine-dependent examples with three trials and are not production claims.

### Parser

| Input | Steady-state-candidate p50 | Timed rate |
|---|---:|---:|
| Single line | 58.594 ns/op | 16.99–17.03 M ops/s |
| Varied lines | 63.156 ns/op | 15.79–15.81 M ops/s |

### In-Memory Text-to-Exchange Path

| Workload | Orders/event | Steady-state-candidate p50 | Timed rate |
|---|---:|---:|---:|
| Mixed | 0.5 | 71.625 ns/event | 13.70–13.78 M events/s |
| All orders | 1.0 | 81.375 ns/event | 12.08–12.11 M events/s |

### Paired Text and Prepared Paths

| Workload | Text p50 | Prepared p50 | Descriptive p50 difference |
|---|---:|---:|---:|
| No orders | 67.062 ns/event | 3.906 ns/event | 63.156 ns/event |
| Mixed | 71.609–79.422 ns/event | 10.422 ns/event | 61.188–69.000 ns/event |
| All orders | 78.125 ns/event | 17.578 ns/event | 60.547 ns/event |

The paired difference is descriptive. It is not exact parser latency.

## Interpretation Boundaries

The timed paths exclude or do not model networking, sockets, file I/O in the timed benchmark path, live exchange protocols, cross-thread communication, exchange latency, fills and matching, production workload distributions, process isolation, and CPU affinity.

The results therefore demonstrate benchmark construction and local path behavior, not exchange-grade performance.

## Engineering Decisions

### Owned Symbols

Queued orders use fixed-size owned symbol storage, removing source-line lifetime dependence.

### Bounded Pipeline

The ring-buffer-backed pipeline has explicit capacity and drop counters.

### Explicit State Machine

Lifecycle transitions are validated rather than inferred from counters alone.

### Structured Rejections

Risk, exchange, amend, and cancel failures preserve exact source and reason.

### Platform-Aware Sanitizers

AppleClang on macOS uses UBSan because AppleClang 17 ASan was observed to deadlock during `dyld` initialization before `main()`. Linux CI retains ASan and UBSan.

### Honest Dynamic-Allocation Boundary

Maps and vectors are used for active orders, lifecycle records, and reports. The repository does not claim a completely allocation-free hot path.

## Production Gap Analysis

A production gateway would still require exchange protocol adapters, network and reconnect state machines, persistent order state and recovery, exchange-assigned order IDs, fills and partial fills, clock discipline, authentication, durable audit output, production configuration controls, explicit threading and CPU placement, bounded or preallocated state stores, and operational monitoring.

## Release Conclusion

The current repository is complete for its intended scope.

It demonstrates staged C++ systems development, deterministic test design, order-state modeling, risk and rejection semantics, benchmark discipline, CI and sanitizer hardening, and explicit engineering boundaries.

The correct release label is a deterministic execution-systems simulator and portfolio artifact, not a production trading gateway.
