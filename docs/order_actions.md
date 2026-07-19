# Cancellation and Amendment Flow

## Purpose

Day 21 adds deterministic cancel and amendment semantics for active exchange orders.

## Active-Order Store

Accepted submissions are stored by sequence ID.

A successful cancellation removes an active order. A successful amendment updates price and quantity while preserving sequence ID, symbol, and side.

## Cancellation Lifecycle

```text
exchange_accepted
→ cancel_pending
→ cancelled
```

A rejected exchange cancellation returns to `exchange_accepted` and preserves its rejection reason.

## Amendment Lifecycle

```text
exchange_accepted
→ amend_pending
→ exchange_accepted
```

A rejected amendment preserves the original active order, returns to `exchange_accepted`, and records the rejection reason.

## Validation

Cancellation requires an active sequence ID.

Amendment requires:

- an active sequence ID
- a finite positive price
- a positive quantity

Illegal lifecycle actions are rejected before exchange dispatch.

## Scope

Day 21 does not add fills, partial fills, asynchronous acknowledgements, network transport, persistence, recovery, or concurrent action handling.
