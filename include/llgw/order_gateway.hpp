#pragma once

#include <cstdint>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"

namespace llgw {

class OrderGateway {
 public:
  explicit OrderGateway(ExchangeSimulator* exchange);

  OrderResponse SendOrder(const OrderRequest& request);

  std::uint64_t orders_sent() const { return orders_sent_; }
  std::uint64_t orders_accepted() const { return orders_accepted_; }
  std::uint64_t orders_rejected() const { return orders_rejected_; }

 private:
  ExchangeSimulator* exchange_ = nullptr;

  std::uint64_t orders_sent_ = 0;
  std::uint64_t orders_accepted_ = 0;
  std::uint64_t orders_rejected_ = 0;
};

}  // namespace llgw
