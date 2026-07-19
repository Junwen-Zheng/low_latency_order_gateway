# Study Log

## Day 1

Built the initial C++ project skeleton for a low-latency order gateway simulator.

Focus areas:

- CMake project setup
- C++20 compiler settings
- Header/source layout
- Fixed-format market-data message representation
- Basic parser using `std::string_view`
- Numeric parsing with `std::from_chars`
- Explicit parser status codes instead of exceptions
- Simple parser tests

Notes:

- I intentionally avoided claiming this is a production trading system.
- The current project only establishes the parser foundation.
- Performance claims will only be added after benchmark methodology exists.

## Day 2

Strengthened parser correctness and added sample feed validation.

Focus areas:

- Exact field-count validation for fixed-format market-data lines
- Stronger malformed-input coverage
- Empty symbol rejection
- Extra/missing field rejection
- Invalid numeric field rejection
- Invalid market-state rejection
- CRLF-style line handling
- Added `data/sample_feed.txt`
- Updated the main executable to validate the sample feed file

Notes:

- I still intentionally avoided benchmark numbers.
- The project now has better parser correctness before any latency claims.
- Locked markets are currently allowed, but crossed markets are rejected.

## Day 3

Added a dedicated deterministic feed replay component.

Focus areas:

- Moved file replay logic out of `main`
- Added `ReplayMarketDataFile`
- Added replay status reporting for file-open failures and parse failures
- Added callback-based replay handling
- Added deterministic replay tests
- Verified replay stops at the first malformed line
- Verified missing-file and empty-file behavior
- Updated `main` to use the replay component

Notes:

- The replay callback receives a `MarketDataUpdate` whose `symbol` field is a `std::string_view` into the current input line.
- Callers that need to store symbol values beyond the callback must copy them.
- No latency or throughput claims are made yet.

## Day 4

Added deterministic order request/response flow.

Focus areas:

- Added order side, order request, order response, and reject reason message types
- Added `ExchangeSimulator`
- Added `OrderGateway`
- Added accept/reject validation for order symbol, price, quantity, and duplicate sequence IDs
- Added gateway counters for sent, accepted, and rejected orders
- Added deterministic tests for exchange simulator and gateway behavior
- Updated main demo to replay market data and send a simple AAPL order

Notes:

- The simulator does not model fills, order books, matching, partial fills, or live exchange connectivity.
- The current order path exists to test deterministic order acceptance/rejection behavior.
- No latency or throughput claims are made yet.

## Day 5

Added deterministic strategy-to-order flow.

Focus areas:

- Added `SimpleStrategy`
- Strategy observes market-data updates and generates a test buy order when spread meets a configured threshold
- Added strategy counters for signals seen and orders generated
- Added strategy unit tests
- Added end-to-end replay → strategy → order gateway → exchange simulator tests
- Verified end-to-end order flow with accepted orders
- Verified replay stops on parse error before later strategy/order processing
- Updated main demo to use `SimpleStrategy`

Notes:

- The strategy is intentionally simple and not intended to represent alpha.
- Its purpose is to exercise the system path from feed replay to deterministic order submission.
- No latency, throughput, profitability, or production-readiness claims are made.

## Day 6

Added a fixed-size ring buffer component.

Focus areas:

- Added templated `RingBuffer<T, Capacity>`
- Added push/pop operations
- Added full/empty/size state tracking
- Added FIFO ordering tests
- Added full-buffer rejection tests
- Added wrap-around ordering tests
- Added tests with both integer and string values

Notes:

- The current ring buffer is correctness-focused and single-threaded.
- It is not yet a lock-free or atomic SPSC queue.
- No latency, throughput, or allocation claims are made yet.

## Day 7

Wired the fixed-size ring buffer into a deterministic order pipeline.

Focus areas:

- Added `OrderPipeline<Capacity>`
- Pipeline uses `RingBuffer<OrderRequest, Capacity>` for queued order requests
- Added enqueue/drop/drain counters
- Added order acceptance/rejection counters at the pipeline level
- Added unit tests for empty drain, accepted drain, full-buffer rejection, and exchange reject tracking
- Added end-to-end feed replay → strategy → pipeline → gateway → exchange tests
- Updated main demo to route strategy-generated orders through the pipeline

Notes:

- The pipeline drains immediately in the main replay callback because `OrderRequest.symbol` is currently a `std::string_view`.
- This avoids storing symbol views beyond the lifetime of the current replay line.
- A future asynchronous pipeline should introduce owned or fixed-width symbol storage before queueing across thread/lifetime boundaries.
- No latency, throughput, lock-free, or production-readiness claims are made.

## Day 8

Performed architecture and documentation cleanup.

Focus areas:

- Added docs/architecture.md
- Added docs/project_status.md
- Updated README to describe current system flow
- Documented current components and responsibilities
- Documented known limitations
- Documented why symbol lifetime must be fixed before async queueing or benchmarking

Notes:

- No production-readiness or latency claims were added.
- This was intentionally a repo-polish and explainability stage.
- The next technical step should be owned or fixed-size symbol storage for queued orders.

## Day 9

Fixed queued order symbol lifetime with owned fixed-size symbol storage.

Focus areas:

- Added `FixedSymbol`
- Replaced `OrderRequest.symbol` from `std::string_view` with owned `FixedSymbol`
- Added tests for valid, empty, too-long, and max-length symbols
- Updated `SimpleStrategy` to copy market-data symbols into owned order symbols
- Updated main demo to enqueue during replay and drain the order pipeline after replay completes
- Updated end-to-end pipeline test to validate queued order lifetime across replay callbacks

Notes:

- `MarketDataUpdate.symbol` is still a `std::string_view` into the feed line.
- `OrderRequest.symbol` is now owned and safe to store in the ring-buffer-backed pipeline.
- This prepares the project for future async/pipeline work without making latency or production-readiness claims.

## Day 10

Added a basic parser benchmark smoke harness and benchmark methodology documentation.

Focus areas:

- Added benchmarks/bench_parser.cpp
- Added scripts/run_benchmarks.sh
- Added docs/benchmark_methodology.md
- Added benchmark target to CMake
- Updated README with benchmark instructions and limitations
- Kept benchmark language conservative and non-claim-based

Notes:

- The benchmark uses std::chrono::steady_clock and a fixed parser input.
- The output is useful as a local smoke measurement only.
- No production latency, exchange-grade, lock-free, or ultra-low-latency claims are made.
- Benchmark results should not be used in resume bullets until methodology is stronger and limitations are clearly documented.

## Day 11

Improved parser benchmark methodology.

Focus areas:

- Added configurable warmup iteration count
- Added configurable measured iteration count
- Added repeated benchmark trials
- Added optional CSV output
- Added benchmark metadata for compiler, compiler version, C++ standard macro, build mode, steady clock behavior, and hardware concurrency
- Updated benchmark methodology documentation
- Updated README benchmark usage
- Ignored local benchmark CSV outputs

Notes:

- The benchmark remains a smoke harness, not a production latency claim.
- Per-iteration timing still includes measurement overhead.
- Results are local, machine-dependent, and should not be used as resume claims yet.

## Day 12

Added benchmark input variety and additional benchmark metadata.

Focus areas:

- Added benchmark input-set selection with `--input-set single` and `--input-set varied`
- Added a varied valid-input benchmark set with multiple symbols, prices, sizes, and spreads
- Added operating system macro metadata
- Added architecture macro metadata
- Added input set and input count to benchmark output
- Added input set and input count to CSV output
- Updated benchmark methodology documentation
- Updated README benchmark examples

Notes:

- The benchmark remains a local smoke harness, not a production latency claim.
- The varied input set improves coverage but is still not a realistic exchange-feed distribution.
- Per-iteration timing still includes measurement overhead.

## Day 13

Replaced per-operation parser timing with batched timing and explicit benchmark calibration information.

Focus areas:

- Added configurable batch size with `--batch-size`
- Timed multiple parser operations per clock interval
- Reported latency as nanoseconds per operation for each timed batch
- Added a separate baseline loop
- Kept baseline and parser results separate instead of automatically subtracting them
- Added clock-pair timing distribution
- Alternated parser and baseline ordering across trials
- Added batch and baseline fields to CSV output
- Updated README and benchmark methodology documentation
- Replaced the stale day-specific test completion message with a generic repository check message

Notes:

- Batched timing reduces timer quantization and per-operation clock overhead.
- The baseline is not a perfect overhead estimate and is not subtracted automatically.
- Results remain local smoke measurements rather than production latency claims.

## Day 14

Added process stabilization and explicit first-trial handling to the parser benchmark.

Focus areas:

- Added configurable process stabilization with `--stabilization-iterations`
- Added an untimed parser stabilization pass before clock calibration and measured trials
- Preserved and labeled the first measured trial as `first_measured`
- Labeled later trials as `steady_state_candidate`
- Added an all-trial descriptive summary
- Added a steady-state candidate summary that excludes the first measured trial
- Kept the first trial visible in console and CSV output
- Added timed parser operations-per-second reporting
- Added stabilization and trial-phase fields to CSV output
- Updated README and benchmark methodology documentation

Notes:

- Stabilization reduces but does not eliminate cold-start, CPU-frequency, thermal, or scheduling effects.
- Timed operations per second is derived only from measured parser-batch duration and is not an end-to-end throughput claim.
- Raw first-trial data remains available rather than being silently discarded.

## Day 15

Added a separate in-memory end-to-end system-path benchmark.

Focus areas:

- Added `benchmarks/bench_end_to_end.cpp`
- Added `scripts/run_end_to_end_benchmark.sh`
- Added `docs/end_to_end_benchmark_methodology.md`
- Added the `bench_end_to_end` CMake target
- Measured text parsing through strategy, order pipeline, gateway, and exchange simulation
- Added `mixed` and `all-orders` workloads
- Added process stabilization and first-trial labeling
- Added batched per-event timing
- Added separate baseline measurements without automatic subtraction
- Added timed events-per-second and orders-per-event fields
- Added pipeline, gateway, and strategy invariant validation
- Added CSV output
- Updated README benchmark documentation

Notes:

- File I/O and `FeedReplay` are intentionally excluded.
- Timed events per second is not file-replay, network, live-exchange, or production throughput.
- Parser microbenchmark results remain separate from system-path benchmark results.
- No production-readiness or latency claims are made.

## Day 16

Added a paired path-comparison benchmark to investigate workload comparability.

Focus areas:

- Added `benchmarks/bench_path_comparison.cpp`
- Added `scripts/run_path_comparison_benchmark.sh`
- Added `docs/path_comparison_benchmark_methodology.md`
- Added the `bench_path_comparison` CMake target
- Compared the full text-parser path with a pre-parsed control path
- Added `no-orders`, `mixed`, and `all-orders` workloads
- Measured both paths within every trial
- Alternated path order across trials
- Warmed each measured system state before timed batches
- Added paired p50 and total-average descriptive differences
- Added strategy, pipeline, gateway, and exchange invariant validation
- Added CSV output
- Updated README and end-to-end benchmark documentation

Notes:

- Paired differences are descriptive and are not exact parser-cost measurements.
- File I/O and FeedReplay remain excluded.
- The benchmark is intended to investigate comparability rather than justify a performance claim.
- The all-orders workload appearing faster in an earlier run is treated as a benchmark question, not a system conclusion.

## Day 17

Consolidated the existing benchmark tooling and documentation.

Focus areas:

- Added scripts/run_benchmark_suite.sh
- Added docs/benchmark_suite.md
- Added quick and full suite modes
- Runs all seven existing benchmark configurations after one build
- Added configurable CSV prefixes and non-empty output validation
- Documented comparison boundaries and prohibited performance claims
- Documented that benchmark values must not become CI performance gates
- Linked the individual benchmark methodology documents
- Updated the README

Notes:

- Day 17 adds no new measurement path.
- Quick mode is a build-and-execution smoke suite.
- Full mode remains a local developer-machine measurement.
- Generated CSV files remain ignored by Git.

## Day 18

Added a lightweight continuous-integration smoke workflow.

Focus areas:

- Added `.github/workflows/ci.yml`
- Added `scripts/run_ci_smoke.sh`
- Added `docs/ci.md`
- Triggered CI on pushes to `main`, pull requests, and manual dispatch
- Used read-only repository permissions
- Added workflow concurrency cancellation
- Added a twenty-minute job timeout
- Ran all repository correctness checks
- Ran the consolidated benchmark suite in quick mode
- Validated seven non-empty benchmark CSV files
- Kept latency and throughput values out of CI pass/fail decisions
- Documented the local equivalent and CI scope boundaries

Notes:

- CI checks successful build, execution, counters, and output creation.
- CI does not enforce permiformance thresholds.
- GitHub-hosted-runner measurements are not directly comparable with local Mac measurements.
- Day 18 adds automation rather than another benchmark path.

## Day 19

Added a deterministic pre-trade risk layer.

Focus areas:

- Added `PreTradeRiskManager`
- Added configurable maximum order quantity
- Added configurable maximum order notional
- Added an optional fixed-capacity symbol allowlist
- Added explicit risk rejection reasons
- Added per-reason risk counters
- Integrated risk checks into `OrderPipeline`
- Added pipeline attribution for risk rejects and gateway rejects
- Ensured risk-rejected orders do not reach the gateway
- Integrated risk into the repository demo path
- Added unit tests for every risk rejection reason
- Added an end-to-end feed, strategy, pipeline, risk, gateway, and exchange test
- Added `docs/pre_trade_risk.md`
- Updated the README

Notes:

- Existing pipeline callers remain compatible because the risk-manager constructor argument is optional.
- The risk layer is deterministic and single-process.
- It does not claim production portfolio or regulatory risk coverage.

## Day 20

Added deterministic order lifecycle tracking.

Focus areas:

- Added `include/llgw/order_lifecycle.hpp`
- Added `src/order_lifecycle.cpp`
- Added `tests/test_order_lifecycle.cpp`
- Added `tests/test_end_to_end_lifecycle.cpp`
- Added `docs/order_lifecycle.md`
- Added created, queued, risk-rejected, sent, exchange-accepted, and exchange-rejected states
- Enforced legal state transitions
- Preserved risk and exchange rejection reasons
- Rejected duplicate lifecycle registration
- Added unknown-order and invalid-transition diagnostics
- Integrated lifecycle tracking into `OrderPipeline`
- Verified that risk-rejected orders never reach the gateway
- Added lifecycle counters to the demo summary
- Preserved compatibility for callers without lifecycle tracking

Notes:

- Terminal states cannot transition again.
- Lifecycle duplicate rejection is distinct from ring-buffer backpressure.
- Day 20 does not yet add cancellation, amendment, or fill states.
- The tracker is single-process and correctness-oriented.

## Day 21

Added deterministic cancellation and amendment flow.

Focus areas:

- Added cancel and amendment request/response types
- Added explicit rejection reasons
- Added an exchange active-order store
- Added active-order lookup
- Added cancellation and amendment validation
- Preserved original orders after rejected amendments
- Added cancel-pending, cancelled, and amend-pending states
- Added lifecycle action counters
- Added gateway action counters
- Rejected illegal lifecycle actions before exchange dispatch
- Added unit and end-to-end action tests
- Added `docs/order_actions.md`
- Updated README and test registration

Notes:

- Sequence IDs remain immutable across amendments.
- Cancelled orders cannot be amended.
- Rejected amendments return to the active accepted state.
- Fills and partial fills remain out of scope.
