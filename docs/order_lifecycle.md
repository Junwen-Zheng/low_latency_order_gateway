# Order Lifecycle Tracking

## Purpose

The lifecycle tracker records deterministic state transitions for each order sequence ID across queueing, pre-trade risk, gateway dispatch, and exchange response handling.

## State Model

```text
created → queued → risk_rejected
```

or:

```text
created → queued → sent → exchange_accepted | exchange_rejected
```

The three rejection or response states are terminal.

## Integration

The tracker is an optional third `OrderPipeline` dependency. Existing callers remain compatible.

## Guarantees

The tracker rejects duplicate registration, unknown-order transitions, invalid transitions, terminal-state transitions, and rejection transitions with a `kNone` reason.

Risk and exchange rejection reasons are preserved in each lifecycle record.

Risk-rejected orders never enter the sent state and never reach the gateway.

## Scope

Day 20 does not yet model cancellations, amendments, partial fills, complete fills, persistence, replay recovery, or cross-thread synchronization.
