#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "llgw/messages.hpp"

namespace llgw {

class ExchangeSimulator {
 public:
  OrderResponse SubmitOrder(const OrderRequest& request);
  CancelResponse CancelOrder(const CancelRequest& request);
  AmendResponse AmendOrder(const AmendRequest& request);

  bool HasActiveOrder(std::uint64_t sequence_id) const;
  std::optional<OrderRequest> FindActiveOrder(
      std::uint64_t sequence_id) const;
  std::size_t active_order_count() const {
    return active_orders_.size();
  }

 private:
  std::unordered_set<std::uint64_t> seen_sequence_ids_;
  std::unordered_map<std::uint64_t, OrderRequest> active_orders_;
};

}  // namespace llgw
