# Architecture

## Scope

The repository models deterministic components commonly found in an electronic-trading execution path.

It is a single-process C++20 simulator. It is intentionally not a live exchange gateway, matching engine, or production risk platform.

## End-to-End Flow

```text
data/sample_feed.txt
        ↓
ReplayMarketDataFile
        ↓
MarketDataParser
        ↓
MarketDataUpdate
        ↓
SimpleStrategy
        ↓
OrderRequest
        ↓
OrderPipeline<Capacity>
        ↓
PreTradeRiskManager
        ↓
OrderGateway
        ↓
ExchangeSimulator
        ↓
OrderResponse
        ↓
OrderLifecycleTracker
        ↓
ExecutionReportJournal
```

Order actions extend the accepted-order path:

```text
ExchangeSimulator active order
        ↓
AmendRequest or CancelRequest
        ↓
OrderGateway
        ↓
ExchangeSimulator
        ↓
lifecycle transition
        ↓
structured execution report
```

## Components

### MarketDataParser

Parses:

```text
SYMBOL,BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE,EXCHANGE_TS_NS
```

Validation includes:

- exact field count
- non-empty symbol
- valid numeric fields
- finite positive prices
- non-zero sizes
- non-crossed market state
- CRLF handling

Failures return explicit statuses rather than exceptions.

### Feed Replay

`ReplayMarketDataFile` reads deterministic input one line at a time and invokes a callback for each valid update.

Replay stops at the first malformed line and reports lines read, updates parsed, error line, and parser status.

### FixedSymbol

`FixedSymbol` provides owned, fixed-capacity symbol storage. Queued `OrderRequest` values therefore do not depend on the lifetime of the source feed line.

### SimpleStrategy

The strategy generates a small test buy order when the spread meets a configured threshold. It exists only to exercise the system path. It is not an alpha model and makes no profitability claim.

### RingBuffer

`RingBuffer<T, Capacity>` is a fixed-capacity FIFO container with push, pop, full and empty checks, size tracking, and wrap-around behavior.

It is correctness-oriented. It is not atomic, lock-free, or thread-safe.

### OrderPipeline

The pipeline owns the fixed-capacity queue and coordinates enqueue and backpressure, pre-trade risk, gateway dispatch, lifecycle transitions, rejection attribution, and counters.

Optional dependencies are non-owning pointers.

### PreTradeRiskManager

Checks symbol validity, finite positive price, positive quantity, maximum quantity, maximum order notional, and an optional fixed-capacity symbol allowlist.

Risk rejections do not reach the gateway.

### OrderGateway

Coordinates order, cancel, and amend requests with the exchange simulator. It tracks accepted and rejected outcomes and integrates action transitions with the lifecycle tracker.

### ExchangeSimulator

Validates submissions and maintains deterministic active-order state. It supports order acceptance and rejection, duplicate sequence prevention, active-order lookup, amendment, and cancellation.

It does not implement matching, fills, queue priority, market impact, or exchange latency.

### OrderLifecycleTracker

Submission states:

```text
created → queued → risk_rejected
created → queued → sent → exchange_rejected
created → queued → sent → exchange_accepted
```

Action states:

```text
exchange_accepted → amend_pending → exchange_accepted
exchange_accepted → cancel_pending → cancelled
```

The tracker rejects duplicate registration, unknown orders, invalid transitions, and terminal-state transitions.

### ExecutionReportJournal

Records successful lifecycle transitions in deterministic order.

Each report contains a monotonic event index, sequence ID, event type, rejection source, and stable rejection reason.

The journal is synchronous and in memory.

## Ownership Model

```text
OrderGateway
  ├─ ExchangeSimulator*
  └─ OrderLifecycleTracker*

OrderPipeline
  ├─ OrderGateway*
  ├─ PreTradeRiskManager*
  └─ OrderLifecycleTracker*

OrderLifecycleTracker
  └─ ExecutionReportJournal*
```

Callers must keep dependencies alive longer than dependants. The demo constructs objects in dependency order so reverse destruction is safe.

## Allocation Boundary

Fixed or bounded structures:

- parser operations
- `FixedSymbol`
- `RingBuffer`
- queued order representation
- risk symbol allowlist

Dynamic structures retained for clarity and observability:

- exchange active-order map
- seen-sequence set
- lifecycle record map
- execution-report vector
- filtered report copies

The repository does not claim a fully allocation-free hot path.

## Concurrency Boundary

All components are single-threaded.

There is no atomic queue, memory-ordering protocol, cross-thread ownership transfer, lock-free claim, or concurrent-writer support.

## Validation Architecture

```text
compiler warnings as errors
        ↓
20 CTest cases
        ↓
platform-supported sanitizers
        ↓
seven-case benchmark smoke suite
        ↓
CSV and invariant validation
```

GitHub Actions runs the same correctness-oriented gate without latency thresholds.

## Deliberate Omissions

- sockets and wire protocols
- reconnect handling
- packet loss and sequence gaps
- persistence and recovery
- fills and partial fills
- matching and order books
- clock synchronization
- production telemetry transport
- production risk governance
- multithreading and CPU affinity
