#include "llgw/pre_trade_risk.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace llgw {

const char* ToString(RiskRejectReason reason) {
  switch (reason) {
    case RiskRejectReason::kNone:
      return "none";
    case RiskRejectReason::kInvalidSymbol:
      return "invalid_symbol";
    case RiskRejectReason::kInvalidPrice:
      return "invalid_price";
    case RiskRejectReason::kInvalidQuantity:
      return "invalid_quantity";
    case RiskRejectReason::kQuantityLimit:
      return "quantity_limit";
    case RiskRejectReason::kNotionalLimit:
      return "notional_limit";
    case RiskRejectReason::kSymbolNotAllowed:
      return "symbol_not_allowed";
  }

  return "unknown";
}

PreTradeRiskManager::PreTradeRiskManager(const RiskLimits& limits)
    : max_order_quantity_(limits.max_order_quantity),
      max_order_notional_(limits.max_order_notional),
      allowed_symbol_count_(
          std::min(limits.allowed_symbol_count, limits.allowed_symbols.size())) {
  for (std::size_t i = 0; i < allowed_symbol_count_; ++i) {
    allowed_symbols_[i] = std::string(limits.allowed_symbols[i]);
  }
}

RiskDecision PreTradeRiskManager::Check(const OrderRequest& request) {
  ++orders_checked_;

  if (request.symbol.empty()) {
    return Reject(RiskRejectReason::kInvalidSymbol);
  }

  if (!std::isfinite(request.price) || request.price <= 0.0) {
    return Reject(RiskRejectReason::kInvalidPrice);
  }

  if (request.quantity == 0) {
    return Reject(RiskRejectReason::kInvalidQuantity);
  }

  if (request.quantity > max_order_quantity_) {
    return Reject(RiskRejectReason::kQuantityLimit);
  }

  const double notional =
      request.price * static_cast<double>(request.quantity);

  if (!std::isfinite(notional) || notional > max_order_notional_) {
    return Reject(RiskRejectReason::kNotionalLimit);
  }

  if (!IsAllowedSymbol(request)) {
    return Reject(RiskRejectReason::kSymbolNotAllowed);
  }

  ++orders_accepted_;

  RiskDecision decision;
  decision.accepted = true;
  decision.reject_reason = RiskRejectReason::kNone;
  return decision;
}

bool PreTradeRiskManager::IsAllowedSymbol(
    const OrderRequest& request) const {
  if (allowed_symbol_count_ == 0) {
    return true;
  }

  for (std::size_t i = 0; i < allowed_symbol_count_; ++i) {
    if (request.symbol == std::string_view(allowed_symbols_[i])) {
      return true;
    }
  }

  return false;
}

RiskDecision PreTradeRiskManager::Reject(RiskRejectReason reason) {
  ++orders_rejected_;

  switch (reason) {
    case RiskRejectReason::kInvalidSymbol:
      ++rejected_invalid_symbol_;
      break;
    case RiskRejectReason::kInvalidPrice:
      ++rejected_invalid_price_;
      break;
    case RiskRejectReason::kInvalidQuantity:
      ++rejected_invalid_quantity_;
      break;
    case RiskRejectReason::kQuantityLimit:
      ++rejected_quantity_limit_;
      break;
    case RiskRejectReason::kNotionalLimit:
      ++rejected_notional_limit_;
      break;
    case RiskRejectReason::kSymbolNotAllowed:
      ++rejected_symbol_not_allowed_;
      break;
    case RiskRejectReason::kNone:
      break;
  }

  RiskDecision decision;
  decision.accepted = false;
  decision.reject_reason = reason;
  return decision;
}

}  // namespace llgw
