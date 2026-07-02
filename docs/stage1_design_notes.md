# Stage 1 Design Notes — Parser and Replay Foundation

## Goal

Stage 1 creates a small, testable foundation for parsing and replaying fixed-format market-data messages in C++.

The goal is not to build a full trading system yet. The goal is to establish a clean message model, parser, feed replay component, tests, and build structure that can later be benchmarked and optimized.

## Current Format

SYMBOL,BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE,EXCHANGE_TS_NS

Example:

AAPL,192.10,100,192.12,200,1710000000123456789

## Design Choices

### `std::string_view`

The parser uses `std::string_view` to avoid copying input fields during parsing.

Parsed symbol values refer to the lifetime of the original input line. This is acceptable during parser and replay callbacks, but callers that need to store symbol values beyond the callback must copy them.

### `std::from_chars`

Numeric parsing uses `std::from_chars` instead of stream parsing. This avoids locale-dependent parsing and is generally more suitable for low-level parsing paths.

### Exact field count

The parser expects exactly six comma-separated fields. Missing fields and extra fields are rejected before numeric parsing.

### Explicit parse status

The parser returns a `ParseStatus` enum instead of throwing exceptions. This keeps error handling explicit and avoids exception paths in parsing logic.

### Market validation

The parser currently rejects:

- non-positive bid or ask prices
- zero bid or ask sizes
- crossed markets where bid price is greater than ask price

Locked markets, where bid price equals ask price, are currently allowed.

### Deterministic file replay

The replay component reads a feed file line by line, parses each line, and invokes a callback for each valid market-data update.

Replay stops at the first malformed line and reports:

- number of lines read
- number of updates parsed
- error line number
- parser status for the malformed line

## Limitations

- No benchmark numbers yet.
- No packet/network input yet.
- No memory allocation instrumentation yet.
- Parser only supports one simple CSV-like fixed format.
- Floating-point parsing behavior depends on standard library support for `std::from_chars(double)`.
- The parser does not trim spaces around fields.
- Replay is single-threaded and file-based.

## Next Steps

- Add deterministic replay scenario tests that validate expected event ordering.
- Add order request / order acknowledgement message types.
- Add benchmark harness only after correctness coverage is stable.
