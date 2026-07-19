#pragma once

#include <cstdint>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"

namespace llgw {

class OrderLifecycleTracker;

class OrderGateway {
 public:
  explicit OrderGateway(
      ExchangeSimulator* exchange,
      OrderLifecycleTracker* lifecycle_tracker = nullptr);

  OrderResponse SendOrder(const OrderRequest& request);
  CancelResponse SendCancel(const CancelRequest& request);
  AmendResponse SendAmend(const AmendRequest& request);

  std::uint64_t orders_sent() const { return orders_sent_; }
  std::uint64_t orders_accepted() const { return orders_accepted_; }
  std::uint64_t orders_rejected() const { return orders_rejected_; }

  std::uint64_t cancels_sent() const { return cancels_sent_; }
  std::uint64_t cancels_accepted() const { return cancels_accepted_; }
  std::uint64_t cancels_rejected() const { return cancels_rejected_; }

  std::uint64_t amends_sent() const { return amends_sent_; }
  std::uint64_t amends_accepted() const { return amends_accepted_; }
  std::uint64_t amends_rejected() const { return amends_rejected_; }

  std::uint64_t lifecycle_transition_errors() const {
    return lifecycle_transition_errors_;
  }

 private:
  ExchangeSimulator* exchange_ = nullptr;
  OrderLifecycleTracker* lifecycle_tracker_ = nullptr;

  std::uint64_t orders_sent_ = 0;
  std::uint64_t orders_accepted_ = 0;
  std::uint64_t orders_rejected_ = 0;

  std::uint64_t cancels_sent_ = 0;
  std::uint64_t cancels_accepted_ = 0;
  std::uint64_t cancels_rejected_ = 0;

  std::uint64_t amends_sent_ = 0;
  std::uint64_t amends_accepted_ = 0;
  std::uint64_t amends_rejected_ = 0;

  std::uint64_t lifecycle_transition_errors_ = 0;
};

}  // namespace llgw
