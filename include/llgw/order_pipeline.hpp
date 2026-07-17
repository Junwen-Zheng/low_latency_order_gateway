#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/pre_trade_risk.hpp"
#include "llgw/ring_buffer.hpp"

namespace llgw {

template <std::size_t Capacity>
class OrderPipeline {
 public:
  explicit OrderPipeline(
      OrderGateway* gateway,
      PreTradeRiskManager* risk_manager = nullptr)
      : gateway_(gateway), risk_manager_(risk_manager) {}

  bool Enqueue(const OrderRequest& request) {
    if (!buffer_.Push(request)) {
      ++orders_dropped_on_enqueue_;
      return false;
    }

    ++orders_enqueued_;
    return true;
  }

  std::size_t Drain() {
    std::size_t drained_now = 0;

    while (true) {
      std::optional<OrderRequest> maybe_order = buffer_.Pop();
      if (!maybe_order.has_value()) {
        break;
      }

      ++orders_drained_;
      ++drained_now;

      if (risk_manager_ != nullptr) {
        const RiskDecision risk_decision =
            risk_manager_->Check(*maybe_order);

        if (!risk_decision.accepted) {
          ++orders_rejected_;
          ++orders_rejected_by_risk_;
          last_risk_reject_reason_ =
              risk_decision.reject_reason;
          continue;
        }
      }

      if (gateway_ == nullptr) {
        ++orders_rejected_;
        ++orders_rejected_by_gateway_;
        continue;
      }

      const OrderResponse response = gateway_->SendOrder(*maybe_order);
      if (response.accepted) {
        ++orders_accepted_;
      } else {
        ++orders_rejected_;
        ++orders_rejected_by_gateway_;
      }
    }

    return drained_now;
  }

  bool Empty() const { return buffer_.Empty(); }
  bool Full() const { return buffer_.Full(); }
  std::size_t Size() const { return buffer_.Size(); }

  std::uint64_t orders_enqueued() const { return orders_enqueued_; }
  std::uint64_t orders_dropped_on_enqueue() const { return orders_dropped_on_enqueue_; }
  std::uint64_t orders_drained() const { return orders_drained_; }
  std::uint64_t orders_accepted() const { return orders_accepted_; }
  std::uint64_t orders_rejected() const { return orders_rejected_; }
  std::uint64_t orders_rejected_by_risk() const {
    return orders_rejected_by_risk_;
  }
  std::uint64_t orders_rejected_by_gateway() const {
    return orders_rejected_by_gateway_;
  }
  RiskRejectReason last_risk_reject_reason() const {
    return last_risk_reject_reason_;
  }

 private:
  RingBuffer<OrderRequest, Capacity> buffer_;
  OrderGateway* gateway_ = nullptr;
  PreTradeRiskManager* risk_manager_ = nullptr;

  std::uint64_t orders_enqueued_ = 0;
  std::uint64_t orders_dropped_on_enqueue_ = 0;
  std::uint64_t orders_drained_ = 0;
  std::uint64_t orders_accepted_ = 0;
  std::uint64_t orders_rejected_ = 0;
  std::uint64_t orders_rejected_by_risk_ = 0;
  std::uint64_t orders_rejected_by_gateway_ = 0;
  RiskRejectReason last_risk_reject_reason_ =
      RiskRejectReason::kNone;
};

}  // namespace llgw
