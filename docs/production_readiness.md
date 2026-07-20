# Production-Readiness Audit

## Purpose

Day 23 adds a repeatable quality gate and documents the boundary between this deterministic execution lab and a production trading gateway.

## Automated Quality Gate

Run:

```bash
./scripts/run_quality_gate.sh
```

The gate performs:

1. a clean release build with all warnings treated as errors
2. the complete CTest suite, including the demo
3. a clean debug build with platform-supported runtime sanitizers
4. the complete CTest suite under those sanitizers

The CI smoke workflow runs this quality gate before the seven-case benchmark smoke suite.

## CTest Registration

Every correctness executable is registered with CTest. The demo is also registered and runs with the repository root as its working directory so `data/sample_feed.txt` resolves deterministically.

Use:

```bash
ctest --test-dir build --output-on-failure
```

## Hardening Coverage

The Day 23 hardening test verifies:

- NaN and infinite submission prices are rejected
- rejected non-finite amendments preserve the original active order
- cancelled sequence IDs cannot be reused
- risk allowlist counts are clamped to fixed storage
- null-exchange amend and cancel failures roll lifecycle state back correctly
- a null gateway produces a deterministic terminal rejection
- unknown execution-report queries do not mutate journal state

## Ownership and Lifetime Audit

The current API uses explicit non-owning pointers for optional components:

- `OrderGateway` does not own `ExchangeSimulator`
- `OrderGateway` does not own `OrderLifecycleTracker`
- `OrderPipeline` does not own the gateway, risk manager, or lifecycle tracker
- `OrderLifecycleTracker` does not own the execution-report journal

Callers must keep these objects alive for the full lifetime of their dependants. The demo constructs objects in dependency order so destruction occurs safely in reverse order.

## Determinism and Allocation Boundary

The parser, fixed symbol, ring buffer, and queued order representation avoid dynamic allocation in their core operations.

The following components intentionally use dynamic containers for correctness and observability:

- exchange active-order and seen-sequence maps
- lifecycle record map
- execution-report vector and filtered report copies

Those choices are acceptable for this research lab but are not presented as allocation-free production hot-path structures.

## Concurrency Boundary

The implementation is single-threaded. No component is thread-safe, lock-free, or safe for concurrent writers.

A production implementation would need an explicit concurrency model, ownership handoff, memory-ordering design, and contention measurements.

## Failure and Recovery Boundary

The project does not provide:

- network transport or reconnect handling
- persistent order state
- crash recovery or replay reconstruction
- exchange-assigned order identifiers
- fills or partial fills
- clock synchronization
- durable audit logging
- authentication or secrets handling
- production risk configuration validation

## Benchmark Boundary

Benchmark results are local measurements only. CI verifies benchmark executability and non-empty CSV output; it does not enforce performance thresholds across heterogeneous runners.

## Readiness Conclusion

The repository is ready as a deterministic, testable portfolio and research artifact.

It is not claimed to be production deployable. The quality gate demonstrates compiler cleanliness, behavioral coverage, and sanitizer cleanliness within the documented single-process scope.

## Platform Sanitizer Policy

The sanitizer configuration is platform-aware:

- AppleClang on macOS runs UndefinedBehaviorSanitizer.
- Clang and GCC on Linux run AddressSanitizer and UndefinedBehaviorSanitizer together.

On the audited macOS environment, AppleClang 17 AddressSanitizer deadlocked
inside the sanitizer runtime during dyld initialization, before `main()`.
The process sample showed recursive allocator interception while ASan was
initializing shadow memory. This is a toolchain/runtime startup failure rather
than a project test failure.

Linux CI remains the AddressSanitizer quality gate. Local macOS validation uses
UndefinedBehaviorSanitizer so the quality gate remains deterministic.
