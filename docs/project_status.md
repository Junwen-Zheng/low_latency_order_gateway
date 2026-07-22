# Project Status

## Release State

The repository is a completed `v1.0.0` release candidate.

Current scope: deterministic single-process execution simulator with correctness tests, benchmark tooling, CI, and a production-boundary audit.

## Completed Capabilities

### Market Data

- fixed-format parser
- explicit parser statuses
- malformed-input coverage
- deterministic file replay
- replay counters and failure location

### Order Path

- simple strategy trigger
- owned fixed-size symbols
- fixed-capacity ring buffer
- order pipeline and backpressure counters
- gateway and exchange simulation
- deterministic rejection reasons

### Risk and State

- maximum quantity
- maximum notional
- optional symbol allowlist
- lifecycle state machine
- duplicate and illegal-transition detection
- active-order storage
- amendment and cancellation
- structured execution reports

### Verification

- 19 correctness and hardening executables
- demo registered as the twentieth CTest case
- warnings-as-errors Release build
- sanitizer build
- platform-aware ASan/UBSan policy
- GitHub Actions quality gate
- seven-case quick benchmark smoke suite
- non-empty CSV validation
- no CI latency thresholds

### Documentation

- architecture
- benchmark methodologies
- CI scope
- risk, lifecycle, actions, and execution reports
- production-readiness audit
- final report
- portfolio summary
- release checklist
- day-by-day study log

## Final Validation Target

Before tagging `v1.0.0`:

```bash
./scripts/run_final_validation.sh
```

Expected high-level result:

```text
20/20 strict CTest cases passed
20/20 sanitizer CTest cases passed
7 non-empty benchmark CSV files
Final validation passed.
```

## Benchmark Status

Benchmark tooling is complete for the current single-process scope.

The stored Day 17 quick-suite artifacts are examples of local measurement output. They are not release gates and must not be compared across machines or environments without matching configuration.

## Production Boundary

The project is ready as a deterministic portfolio artifact, a C++ systems-design discussion project, a benchmark-methodology exercise, and a base for future execution research.

It is not ready as a live exchange gateway, production risk system, multithreaded low-latency engine, persistent order-management system, or matching engine.

## Post-v1 Ideas

Post-v1 work should be a separate phase, not required for this release:

- explicit SPSC threading model
- bounded lifecycle and report storage
- exchange-assigned identifiers
- fill and partial-fill events
- persistence and deterministic recovery replay
- Linux CPU-affinity experiments
- allocation instrumentation
- binary protocol encoding

These are not blockers for `v1.0.0`.
