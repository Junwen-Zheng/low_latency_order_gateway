# Portfolio Summary

## One-Sentence Description

Built a deterministic C++20 order-gateway simulator covering market-data parsing, bounded order queueing, pre-trade risk, exchange simulation, lifecycle enforcement, cancel/amend flows, structured execution reports, benchmarking, and CI hardening.

## Resume Bullets

- Built a C++20 execution simulator spanning fixed-format market-data parsing, deterministic replay, strategy-triggered orders, bounded queueing, pre-trade risk, gateway dispatch, and exchange acknowledgement.
- Implemented owned fixed-size symbols, a fixed-capacity ring buffer, explicit rejection reasons, lifecycle state enforcement, active-order tracking, amendment, cancellation, and structured execution reports.
- Added 20 CTest cases, warnings-as-errors builds, platform-aware sanitizer validation, GitHub Actions, hardening tests, and a repeatable final quality gate.
- Designed parser, in-memory system-path, and paired-path benchmarks with stabilization, batched timing, repeated trials, CSV output, invariant checks, and documented interpretation limits.

## Interview Walkthrough

### Problem

Model the core control flow of an execution gateway without hiding correctness issues behind premature optimization.

### Architecture

```text
feed → parser → strategy → pipeline → risk → gateway → exchange
                                      ↓
                         lifecycle and execution reports
```

### Key Decisions

- fixed-size owned symbols for queued requests
- fixed-capacity pipeline with explicit backpressure
- deterministic state transitions
- exact rejection-source attribution
- separate correctness and benchmark concerns
- no benchmark thresholds in heterogeneous CI
- platform-aware sanitizer policy

### Strongest Technical Discussion Points

- symbol lifetime bug prevention
- queue capacity and drop accounting
- risk rejection before gateway dispatch
- terminal lifecycle states
- cancel/amend rollback behavior
- preserving rejected amendment state
- benchmark baseline limits
- why paired-path subtraction is descriptive rather than exact
- ASan runtime diagnosis on AppleClang
- production gaps around persistence, networking, and concurrency

## Honest Positioning

Use:

> Deterministic C++20 execution-systems simulator with a bounded order path, risk controls, lifecycle tracking, order actions, structured reports, tests, benchmarks, and CI hardening.

Do not use:

- production exchange gateway
- lock-free trading engine
- exchange-grade latency
- live trading platform
- profitable strategy
- allocation-free system

## Repository Demonstration Order

1. `README.md`
2. `docs/architecture.md`
3. `include/llgw/order_pipeline.hpp`
4. `include/llgw/pre_trade_risk.hpp`
5. `include/llgw/order_lifecycle.hpp`
6. `include/llgw/execution_report.hpp`
7. end-to-end tests
8. `docs/production_readiness.md`
9. benchmark methodology
10. `docs/final_report.md`
