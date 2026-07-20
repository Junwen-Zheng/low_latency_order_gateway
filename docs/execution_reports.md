# Structured Execution Reports

## Purpose

Day 22 adds a deterministic in-memory execution-report journal.

The journal converts lifecycle transitions into ordered structured events for tests, diagnostics, and future transport or persistence work.

## Report Schema

Each report contains:

- monotonic one-based event index
- order sequence ID
- event type
- rejection source
- stable rejection reason string

Example:

```text
event=5 sequence_id=42 type=exchange_rejected reject_source=exchange reject_reason=duplicate_sequence
```

Non-rejection events use `reject_source=none` and `reject_reason=none`.

## Deterministic Ordering

A normal submit, amend, and cancel flow produces:

```text
created
queued
sent
exchange_accepted
amend_pending
amend_accepted
cancel_pending
cancelled
```

Global event indexes preserve the exact interleaving across sequence IDs.

## Rejection Fields

Reports preserve specific risk, exchange, amend, and cancel rejection reasons rather than replacing them with a generic failure string.

## Integration

The journal is optional:

```cpp
llgw::ExecutionReportJournal reports;
llgw::OrderLifecycleTracker lifecycle(&reports);
```

Existing callers can continue constructing `OrderLifecycleTracker` without a journal.

## Counters and Queries

The journal exposes total reports, rejection reports, submission reports, action reports, immutable ordered access, and sequence-ID filtering.

## Scope

Day 22 does not add timestamps, binary encoding, asynchronous delivery, persistence, recovery, concurrent writers, fills, or partial fills.
