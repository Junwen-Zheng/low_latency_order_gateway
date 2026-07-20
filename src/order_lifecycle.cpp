#include "llgw/order_lifecycle.hpp"

namespace llgw {

OrderLifecycleTracker::OrderLifecycleTracker(
    ExecutionReportJournal* execution_reports)
    : execution_reports_(execution_reports) {}

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
    case OrderLifecycleState::kCancelPending:
      return "cancel_pending";
    case OrderLifecycleState::kCancelled:
      return "cancelled";
    case OrderLifecycleState::kAmendPending:
      return "amend_pending";
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

  const auto result = records_.emplace(sequence_id, record);

  if (!result.second) {
    return Fail(LifecycleError::kDuplicateOrder);
  }

  ++orders_registered_;
  last_error_ = LifecycleError::kNone;

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kCreated);
  }

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

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kQueued);
  }

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

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordRiskRejected(
        sequence_id,
        reason);
  }

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

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kSent);
  }

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

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kExchangeAccepted);
  }

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

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordExchangeRejected(
        sequence_id,
        reason);
  }

  return true;
}

bool OrderLifecycleTracker::MarkCancelPending(
    std::uint64_t sequence_id) {
  if (!Transition(
          sequence_id,
          OrderLifecycleState::kExchangeAccepted,
          OrderLifecycleState::kCancelPending)) {
    return false;
  }

  ++cancel_requests_;

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kCancelPending);
  }

  return true;
}

bool OrderLifecycleTracker::MarkCancelled(
    std::uint64_t sequence_id) {
  if (!Transition(
          sequence_id,
          OrderLifecycleState::kCancelPending,
          OrderLifecycleState::kCancelled)) {
    return false;
  }

  ++cancels_accepted_;

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kCancelled);
  }

  return true;
}

bool OrderLifecycleTracker::MarkCancelRejected(
    std::uint64_t sequence_id,
    CancelRejectReason reason) {
  if (reason == CancelRejectReason::kNone) {
    return Fail(LifecycleError::kInvalidTransition);
  }

  if (!Transition(
          sequence_id,
          OrderLifecycleState::kCancelPending,
          OrderLifecycleState::kExchangeAccepted)) {
    return false;
  }

  records_.at(sequence_id).cancel_reject_reason = reason;
  ++cancels_rejected_;

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordCancelRejected(
        sequence_id,
        reason);
  }

  return true;
}

bool OrderLifecycleTracker::MarkAmendPending(
    std::uint64_t sequence_id) {
  if (!Transition(
          sequence_id,
          OrderLifecycleState::kExchangeAccepted,
          OrderLifecycleState::kAmendPending)) {
    return false;
  }

  ++amend_requests_;

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kAmendPending);
  }

  return true;
}

bool OrderLifecycleTracker::MarkAmendAccepted(
    std::uint64_t sequence_id) {
  if (!Transition(
          sequence_id,
          OrderLifecycleState::kAmendPending,
          OrderLifecycleState::kExchangeAccepted)) {
    return false;
  }

  records_.at(sequence_id).amend_reject_reason =
      AmendRejectReason::kNone;
  ++amends_accepted_;

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordState(
        sequence_id,
        ExecutionReportType::kAmendAccepted);
  }

  return true;
}

bool OrderLifecycleTracker::MarkAmendRejected(
    std::uint64_t sequence_id,
    AmendRejectReason reason) {
  if (reason == AmendRejectReason::kNone) {
    return Fail(LifecycleError::kInvalidTransition);
  }

  if (!Transition(
          sequence_id,
          OrderLifecycleState::kAmendPending,
          OrderLifecycleState::kExchangeAccepted)) {
    return false;
  }

  records_.at(sequence_id).amend_reject_reason = reason;
  ++amends_rejected_;

  if (execution_reports_ != nullptr) {
    execution_reports_->RecordAmendRejected(
        sequence_id,
        reason);
  }

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
