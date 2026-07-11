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
