#pragma once

#include <cstdint>
#include <optional>

#include "llgw/messages.hpp"

namespace llgw {

class SimpleStrategy {
 public:
  explicit SimpleStrategy(double min_spread);

  std::optional<OrderRequest> OnMarketData(const MarketDataUpdate& update);

  std::uint64_t signals_seen() const { return signals_seen_; }
  std::uint64_t orders_generated() const { return orders_generated_; }

 private:
  double min_spread_ = 0.0;
  std::uint64_t next_sequence_id_ = 1;

  std::uint64_t signals_seen_ = 0;
  std::uint64_t orders_generated_ = 0;
};

}  // namespace llgw
