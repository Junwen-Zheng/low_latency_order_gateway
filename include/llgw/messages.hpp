#pragma once

#include <cstdint>
#include <string_view>

namespace llgw {

struct MarketDataUpdate {
  std::string_view symbol;

  double bid_price = 0.0;
  std::uint32_t bid_size = 0;

  double ask_price = 0.0;
  std::uint32_t ask_size = 0;

  std::uint64_t exchange_ts_ns = 0;
};

struct OrderRequest {
  std::uint64_t sequence_id = 0;
  std::string_view symbol;

  double price = 0.0;
  std::uint32_t quantity = 0;
};

}  // namespace llgw
