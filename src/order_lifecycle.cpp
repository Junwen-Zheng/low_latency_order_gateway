#include "llgw/order_lifecycle.hpp"

namespace llgw {

const char* ToString(OrderLifecycleState state) {
  switch (state) {
    case OrderLifecycleState::kCreated:
      return "created";
    case OrderLifecycleState::kQueued:
      return "queued";
    case OrderLifecycleState::kRiskRejected:
      return "risk_rejected";
    case OrderLifecycleState::kSent:
      return "sent";
    case OrderLifecycleState::kExchangeAccepted:
      return "exchange_accepted";
    case OrderLifecycleState::kExchangeRejected:
      return "exchange_rejected";
  }

  return "unknown";
}

const char* ToString(LifecycleError error) {
  switch (error) {
    case LifecycleError::kNone:
      return "none";
    case LifecycleError::kDuplicateOrder:
      return "duplicate_order";
    case LifecycleError::kUnknownOrder:
      return "unknown_order";
    case LifecycleError::kInvalidTransition:
      return "invalid_transition";
  }

  return "unknown";
}

bool OrderLifecycleTracker::RegisterCreated(
    std::uint64_t sequence_id) {
  OrderLifecycleRecord record;
  record.sequence_id = sequence_id;

  const auto [_, inserted] =
      records_.emplace(sequence_id, record);

  if (!inserted) {
    return Fail(LifecycleError::kDuplicateOrder);
  }

  ++orders_registered_;
  last_error_ = LifecycleError::kNone;
  return true;
}

bool OrderLifecycleTracker::RegisterQueued(
    std::uint64_t sequence_id) {
  if (!RegisterCreated(sequence_id)) {
    return false;
  }

  return MarkQueued(sequence_id);
}

bool OrderLifecycleTracker::MarkQueued(
    std::uint64_t sequence_id) {
  if (!Transition(
          sequence_id,
          OrderLifecycleState::kCreated,
          OrderLifecycleState::kQueued)) {
    return false;
  }

  ++orders_queued_;
  return true;
}

bool OrderLifecycleTracker::MarkRiskRejected(
    std::uint64_t sequence_id,
    RiskRejectReason reason) {
  if (reason == RiskRejectReason::kNone) {
    return Fail(LifecycleError::kInvalidTransition);
  }

  if (!Transition(
          sequence_id,
          OrderLifecycleState::kQueued,
          OrderLifecycleState::kRiskRejected)) {
    return false;
  }

  records_.at(sequence_id).risk_reject_reason = reason;
  ++orders_risk_rejected_;
  return true;
}

bool OrderLifecycleTracker::MarkSent(
    std::uint64_t sequence_id) {
  if (!Transition(
          sequence_id,
          OrderLifecycleState::kQueued,
          OrderLifecycleState::kSent)) {
    return false;
  }

  ++orders_sent_;
  return true;
}

bool OrderLifecycleTracker::MarkExchangeAccepted(
    std::uint64_t sequence_id) {
  if (!Transition(
          sequence_id,
          OrderLifecycleState::kSent,
          OrderLifecycleState::kExchangeAccepted)) {
    return false;
  }

  ++orders_exchange_accepted_;
  return true;
}

bool OrderLifecycleTracker::MarkExchangeRejected(
    std::uint64_t sequence_id,
    OrderRejectReason reason) {
  if (reason == OrderRejectReason::kNone) {
    return Fail(LifecycleError::kInvalidTransition);
  }

  if (!Transition(
          sequence_id,
          OrderLifecycleState::kSent,
          OrderLifecycleState::kExchangeRejected)) {
    return false;
  }

  records_.at(sequence_id).exchange_reject_reason = reason;
  ++orders_exchange_rejected_;
  return true;
}

bool OrderLifecycleTracker::Contains(
    std::uint64_t sequence_id) const {
  return records_.find(sequence_id) != records_.end();
}

std::optional<OrderLifecycleRecord>
OrderLifecycleTracker::Find(
    std::uint64_t sequence_id) const {
  const auto it = records_.find(sequence_id);
  if (it == records_.end()) {
    return std::nullopt;
  }

  return it->second;
}

bool OrderLifecycleTracker::Transition(
    std::uint64_t sequence_id,
    OrderLifecycleState expected,
    OrderLifecycleState next) {
  const auto it = records_.find(sequence_id);

  if (it == records_.end()) {
    return Fail(LifecycleError::kUnknownOrder);
  }

  if (it->second.state != expected) {
    return Fail(LifecycleError::kInvalidTransition);
  }

  it->second.state = next;
  last_error_ = LifecycleError::kNone;
  return true;
}

bool OrderLifecycleTracker::Fail(
    LifecycleError error) {
  ++transition_errors_;
  last_error_ = error;

  switch (error) {
    case LifecycleError::kDuplicateOrder:
      ++duplicate_order_errors_;
      break;
    case LifecycleError::kUnknownOrder:
      ++unknown_order_errors_;
      break;
    case LifecycleError::kInvalidTransition:
      ++invalid_transition_errors_;
      break;
    case LifecycleError::kNone:
      break;
  }

  return false;
}

}  // namespace llgw
