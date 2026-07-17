#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "llgw/messages.hpp"

namespace llgw {

enum class RiskRejectReason {
  kNone,
  kInvalidSymbol,
  kInvalidPrice,
  kInvalidQuantity,
  kQuantityLimit,
  kNotionalLimit,
  kSymbolNotAllowed,
};

struct RiskDecision {
  bool accepted = false;
  RiskRejectReason reject_reason = RiskRejectReason::kNone;
};

struct RiskLimits {
  static constexpr std::size_t kMaxAllowedSymbols = 8;

  std::uint32_t max_order_quantity = 1000;
  double max_order_notional = 1'000'000.0;
  std::array<std::string_view, kMaxAllowedSymbols> allowed_symbols{};
  std::size_t allowed_symbol_count = 0;
};

const char* ToString(RiskRejectReason reason);

class PreTradeRiskManager {
 public:
  explicit PreTradeRiskManager(const RiskLimits& limits);

  RiskDecision Check(const OrderRequest& request);

  std::uint64_t orders_checked() const { return orders_checked_; }
  std::uint64_t orders_accepted() const { return orders_accepted_; }
  std::uint64_t orders_rejected() const { return orders_rejected_; }

  std::uint64_t rejected_invalid_symbol() const {
    return rejected_invalid_symbol_;
  }
  std::uint64_t rejected_invalid_price() const {
    return rejected_invalid_price_;
  }
  std::uint64_t rejected_invalid_quantity() const {
    return rejected_invalid_quantity_;
  }
  std::uint64_t rejected_quantity_limit() const {
    return rejected_quantity_limit_;
  }
  std::uint64_t rejected_notional_limit() const {
    return rejected_notional_limit_;
  }
  std::uint64_t rejected_symbol_not_allowed() const {
    return rejected_symbol_not_allowed_;
  }

 private:
  bool IsAllowedSymbol(const OrderRequest& request) const;
  RiskDecision Reject(RiskRejectReason reason);

  std::uint32_t max_order_quantity_ = 0;
  double max_order_notional_ = 0.0;
  std::array<std::string, RiskLimits::kMaxAllowedSymbols> allowed_symbols_{};
  std::size_t allowed_symbol_count_ = 0;

  std::uint64_t orders_checked_ = 0;
  std::uint64_t orders_accepted_ = 0;
  std::uint64_t orders_rejected_ = 0;
  std::uint64_t rejected_invalid_symbol_ = 0;
  std::uint64_t rejected_invalid_price_ = 0;
  std::uint64_t rejected_invalid_quantity_ = 0;
  std::uint64_t rejected_quantity_limit_ = 0;
  std::uint64_t rejected_notional_limit_ = 0;
  std::uint64_t rejected_symbol_not_allowed_ = 0;
};

}  // namespace llgw
