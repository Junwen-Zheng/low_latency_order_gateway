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
