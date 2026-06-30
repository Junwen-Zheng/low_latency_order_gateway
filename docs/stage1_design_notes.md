# Stage 1 Design Notes — Parser Foundation

## Goal

Stage 1 creates a small, testable foundation for parsing fixed-format market-data messages in C++.

The goal is not to build a full trading system yet. The goal is to establish a clean message model, parser, tests, and build structure that can later be benchmarked and optimized.

## Current Format

SYMBOL,BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE,EXCHANGE_TS_NS

Example:

AAPL,192.10,100,192.12,200,1710000000123456789

## Design Choices

### std::string_view

The parser uses std::string_view to avoid copying input fields during parsing.

This means parsed symbol values refer to the lifetime of the original input line. This is acceptable for the current parser tests and simple replay model, but later pipeline stages must ensure that message storage lifetimes are safe.

### std::from_chars

Numeric parsing uses std::from_chars instead of stream parsing. This avoids locale-dependent parsing and is generally more suitable for low-level parsing paths.

### Explicit Parse Status

The parser returns a ParseStatus enum instead of throwing exceptions. This keeps error handling explicit and avoids exception paths in parsing logic.

## Limitations

- No benchmark numbers yet.
- No packet/network input yet.
- No memory allocation instrumentation yet.
- Parser only supports one simple CSV-like fixed format.
- Floating-point parsing behavior depends on standard library support for std::from_chars(double).

## Next Steps

- Add GoogleTest or keep simple tests until the project grows.
- Add benchmark harness for parser latency.
- Add malformed input coverage.
- Add deterministic replay from file.
