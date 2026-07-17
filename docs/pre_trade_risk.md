# Pre-Trade Risk Layer

## Purpose

The pre-trade risk layer validates an order before it reaches the order gateway or exchange simulator.

The integrated path is:

```text
strategy
→ order pipeline
→ pre-trade risk manager
→ order gateway
→ exchange simulator
```

## Checks

The deterministic risk manager checks orders in this order:

1. non-empty symbol
2. finite positive price
3. positive quantity
4. maximum order quantity
5. maximum order notional
6. optional symbol allowlist

An empty allowlist allows any otherwise-valid non-empty symbol.

## Decisions

Each check returns a `RiskDecision` containing:

- `accepted`
- `RiskRejectReason`

Supported risk rejection reasons are:

- `invalid_symbol`
- `invalid_price`
- `invalid_quantity`
- `quantity_limit`
- `notional_limit`
- `symbol_not_allowed`

## Counters

The risk manager records:

- orders checked
- orders accepted
- orders rejected
- rejection count for each reason

The order pipeline separately records:

- total accepted and rejected orders
- risk-layer rejects
- gateway or exchange rejects

Risk-rejected orders do not reach the gateway.

## Scope

This is a deterministic educational risk layer.

It does not implement:

- account-level exposure
- portfolio aggregation
- market-data freshness checks
- price collars relative to a reference market
- regulatory controls
- persistence
- distributed risk state
- production exchange controls
