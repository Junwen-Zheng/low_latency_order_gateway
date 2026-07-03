#pragma once

#include <cstdint>
#include <unordered_set>

#include "llgw/messages.hpp"

namespace llgw {

class ExchangeSimulator {
 public:
  OrderResponse SubmitOrder(const OrderRequest& request);

 private:
  std::unordered_set<std::uint64_t> seen_sequence_ids_;
};

}  // namespace llgw
