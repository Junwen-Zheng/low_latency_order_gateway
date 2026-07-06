# Architecture Notes

## Project Scope

This project is a C++20 learning project focused on deterministic components commonly found in latency-sensitive trading infrastructure:

- fixed-format market-data parsing
- deterministic feed replay
- simple strategy-to-order flow
- order gateway simulation
- exchange-side accept/reject simulation
- fixed-size ring-buffer-backed order pipeline
- correctness-focused tests

The project is intentionally scoped as a simulator. It is not connected to a live exchange and does not implement a real trading strategy.

## Current Data Flow

sample feed file
    ↓
ReplayMarketDataFile
    ↓
MarketDataUpdate
    ↓
SimpleStrategy
    ↓
OrderRequest
    ↓
OrderPipeline
    ↓
OrderGateway
    ↓
ExchangeSimulator
    ↓
OrderResponse

## Components

### MarketDataParser

Parses fixed-format market-data lines:

SYMBOL,BID_PRICE,BID_SIZE,ASK_PRICE,ASK_SIZE,EXCHANGE_TS_NS

The parser validates:

- exact field count
- non-empty symbol
- numeric fields
- positive prices
- non-zero sizes
- non-crossed market state

It returns explicit ParseStatus values instead of throwing exceptions.

### FeedReplay

ReplayMarketDataFile reads a feed file line by line and invokes a callback for each valid market-data update.

Replay stops at the first malformed line and reports:

- lines read
- updates parsed
- error line
- parser status

### SimpleStrategy

SimpleStrategy is intentionally simple. It generates a test buy order when the bid/ask spread meets a configured threshold.

It is not intended to represent alpha. Its purpose is to exercise the deterministic system path from market data to order submission.

### ExchangeSimulator

ExchangeSimulator accepts or rejects orders deterministically.

It currently rejects:

- empty symbols
- non-positive prices
- zero quantity
- duplicate sequence IDs

It does not model order books, fills, partial fills, matching, latency, or live exchange connectivity.

### OrderGateway

OrderGateway sends orders to the exchange simulator and tracks:

- orders sent
- orders accepted
- orders rejected

### RingBuffer

RingBuffer<T, Capacity> is a fixed-size, correctness-focused ring buffer.

It currently supports:

- push
- pop
- empty/full checks
- size tracking
- wrap-around behavior

It is not currently a lock-free or atomic SPSC queue.

### OrderPipeline

OrderPipeline<Capacity> uses RingBuffer<OrderRequest, Capacity> to queue order requests before draining them to the order gateway.

It tracks:

- orders enqueued
- orders dropped on enqueue
- orders drained
- orders accepted
- orders rejected

## Current Important Limitation

MarketDataUpdate.symbol and OrderRequest.symbol currently use std::string_view.

This means queued orders must not outlive the feed line that produced them. In the current main demo, the pipeline drains immediately inside the replay callback to avoid symbol lifetime issues.

Before introducing async queueing or cross-thread handoff, OrderRequest.symbol should be changed to owned or fixed-size symbol storage.

## Testing Philosophy

The project currently prioritizes correctness before performance claims.

Tests cover:

- parser correctness
- malformed input handling
- feed replay behavior
- exchange simulator rejection paths
- order gateway counters
- simple strategy behavior
- end-to-end order flow
- ring buffer behavior
- order pipeline behavior

## What This Project Does Not Claim

This project does not claim to be:

- production-ready
- exchange-grade
- ultra-low-latency
- lock-free
- profitable
- connected to live markets
- a real trading strategy

Performance claims should only be added after benchmark methodology, reproducibility notes, and measurement limitations are documented.
