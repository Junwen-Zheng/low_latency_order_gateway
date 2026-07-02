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
