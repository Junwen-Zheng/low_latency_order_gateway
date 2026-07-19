#pragma once

#include <cstdint>
#include <string_view>

#include "llgw/fixed_symbol.hpp"

namespace llgw {

enum class Side {
  kBuy,
  kSell,
};

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
  FixedSymbol symbol;
  Side side = Side::kBuy;

  double price = 0.0;
  std::uint32_t quantity = 0;
};

enum class OrderRejectReason {
  kNone,
  kInvalidSymbol,
  kInvalidPrice,
  kInvalidQuantity,
  kDuplicateSequence,
};


enum class CancelRejectReason {
  kNone,
  kUnknownOrder,
  kInvalidState,
};

struct CancelRequest {
  std::uint64_t sequence_id = 0;
};

struct CancelResponse {
  std::uint64_t sequence_id = 0;
  bool accepted = false;
  CancelRejectReason reject_reason = CancelRejectReason::kNone;
};

enum class AmendRejectReason {
  kNone,
  kUnknownOrder,
  kInvalidPrice,
  kInvalidQuantity,
  kInvalidState,
};

struct AmendRequest {
  std::uint64_t sequence_id = 0;
  double new_price = 0.0;
  std::uint32_t new_quantity = 0;
};

struct AmendResponse {
  std::uint64_t sequence_id = 0;
  bool accepted = false;
  AmendRejectReason reject_reason = AmendRejectReason::kNone;
};

struct OrderResponse {
  std::uint64_t sequence_id = 0;
  bool accepted = false;
  OrderRejectReason reject_reason = OrderRejectReason::kNone;
};

}  // namespace llgw
