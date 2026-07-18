#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "llgw/messages.hpp"
#include "llgw/pre_trade_risk.hpp"

namespace llgw {

enum class OrderLifecycleState {
  kCreated,
  kQueued,
  kRiskRejected,
  kSent,
  kExchangeAccepted,
  kExchangeRejected,
};

enum class LifecycleError {
  kNone,
  kDuplicateOrder,
  kUnknownOrder,
  kInvalidTransition,
};

struct OrderLifecycleRecord {
  std::uint64_t sequence_id = 0;
  OrderLifecycleState state = OrderLifecycleState::kCreated;
  RiskRejectReason risk_reject_reason = RiskRejectReason::kNone;
  OrderRejectReason exchange_reject_reason = OrderRejectReason::kNone;
};

const char* ToString(OrderLifecycleState state);
const char* ToString(LifecycleError error);

class OrderLifecycleTracker {
 public:
  bool RegisterCreated(std::uint64_t sequence_id);
  bool RegisterQueued(std::uint64_t sequence_id);

  bool MarkQueued(std::uint64_t sequence_id);
  bool MarkRiskRejected(
      std::uint64_t sequence_id,
      RiskRejectReason reason);
  bool MarkSent(std::uint64_t sequence_id);
  bool MarkExchangeAccepted(std::uint64_t sequence_id);
  bool MarkExchangeRejected(
      std::uint64_t sequence_id,
      OrderRejectReason reason);

  bool Contains(std::uint64_t sequence_id) const;
  std::optional<OrderLifecycleRecord> Find(
      std::uint64_t sequence_id) const;

  std::uint64_t orders_registered() const { return orders_registered_; }
  std::uint64_t orders_queued() const { return orders_queued_; }
  std::uint64_t orders_risk_rejected() const { return orders_risk_rejected_; }
  std::uint64_t orders_sent() const { return orders_sent_; }
  std::uint64_t orders_exchange_accepted() const {
    return orders_exchange_accepted_;
  }
  std::uint64_t orders_exchange_rejected() const {
    return orders_exchange_rejected_;
  }

  std::uint64_t transition_errors() const { return transition_errors_; }
  std::uint64_t duplicate_order_errors() const {
    return duplicate_order_errors_;
  }
  std::uint64_t unknown_order_errors() const {
    return unknown_order_errors_;
  }
  std::uint64_t invalid_transition_errors() const {
    return invalid_transition_errors_;
  }

  LifecycleError last_error() const { return last_error_; }

 private:
  bool Transition(
      std::uint64_t sequence_id,
      OrderLifecycleState expected,
      OrderLifecycleState next);
  bool Fail(LifecycleError error);

  std::unordered_map<std::uint64_t, OrderLifecycleRecord> records_;

  std::uint64_t orders_registered_ = 0;
  std::uint64_t orders_queued_ = 0;
  std::uint64_t orders_risk_rejected_ = 0;
  std::uint64_t orders_sent_ = 0;
  std::uint64_t orders_exchange_accepted_ = 0;
  std::uint64_t orders_exchange_rejected_ = 0;

  std::uint64_t transition_errors_ = 0;
  std::uint64_t duplicate_order_errors_ = 0;
  std::uint64_t unknown_order_errors_ = 0;
  std::uint64_t invalid_transition_errors_ = 0;

  LifecycleError last_error_ = LifecycleError::kNone;
};

}  // namespace llgw
