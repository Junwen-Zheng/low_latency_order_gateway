#include <cstdlib>
#include <iostream>
#include <string_view>

#include "llgw/messages.hpp"
#include "llgw/pre_trade_risk.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::RiskLimits MakeLimits() {
  llgw::RiskLimits limits;
  limits.max_order_quantity = 100;
  limits.max_order_notional = 10'000.0;
  limits.allowed_symbols[0] = "AAPL";
  limits.allowed_symbols[1] = "MSFT";
  limits.allowed_symbol_count = 2;
  return limits;
}

llgw::OrderRequest MakeOrder(
    std::string_view symbol,
    double price,
    std::uint32_t quantity) {
  llgw::OrderRequest request{};
  request.sequence_id = 1;
  request.symbol = symbol;
  request.side = llgw::Side::kBuy;
  request.price = price;
  request.quantity = quantity;
  return request;
}

void TestValidOrderPasses() {
  llgw::PreTradeRiskManager risk(MakeLimits());

  const llgw::RiskDecision decision =
      risk.Check(MakeOrder("AAPL", 100.0, 100));

  Expect(decision.accepted, "valid order should pass risk checks");
  Expect(
      decision.reject_reason == llgw::RiskRejectReason::kNone,
      "valid order should have no reject reason");
  Expect(risk.orders_checked() == 1, "one order should be checked");
  Expect(risk.orders_accepted() == 1, "one order should be accepted");
  Expect(risk.orders_rejected() == 0, "zero orders should be rejected");
}

void TestExplicitRejectReasonsAndCounters() {
  llgw::PreTradeRiskManager risk(MakeLimits());

  const llgw::RiskDecision invalid_symbol =
      risk.Check(MakeOrder("", 100.0, 10));
  const llgw::RiskDecision invalid_price =
      risk.Check(MakeOrder("AAPL", 0.0, 10));
  const llgw::RiskDecision invalid_quantity =
      risk.Check(MakeOrder("AAPL", 100.0, 0));
  const llgw::RiskDecision quantity_limit =
      risk.Check(MakeOrder("AAPL", 100.0, 101));
  const llgw::RiskDecision notional_limit =
      risk.Check(MakeOrder("MSFT", 100.01, 100));
  const llgw::RiskDecision symbol_not_allowed =
      risk.Check(MakeOrder("NVDA", 100.0, 10));

  Expect(
      invalid_symbol.reject_reason ==
          llgw::RiskRejectReason::kInvalidSymbol,
      "empty symbol should be rejected as invalid symbol");
  Expect(
      invalid_price.reject_reason ==
          llgw::RiskRejectReason::kInvalidPrice,
      "zero price should be rejected as invalid price");
  Expect(
      invalid_quantity.reject_reason ==
          llgw::RiskRejectReason::kInvalidQuantity,
      "zero quantity should be rejected as invalid quantity");
  Expect(
      quantity_limit.reject_reason ==
          llgw::RiskRejectReason::kQuantityLimit,
      "oversized quantity should hit the quantity limit");
  Expect(
      notional_limit.reject_reason ==
          llgw::RiskRejectReason::kNotionalLimit,
      "oversized notional should hit the notional limit");
  Expect(
      symbol_not_allowed.reject_reason ==
          llgw::RiskRejectReason::kSymbolNotAllowed,
      "disallowed symbol should be rejected");

  Expect(risk.orders_checked() == 6, "six orders should be checked");
  Expect(risk.orders_accepted() == 0, "no invalid orders should pass");
  Expect(risk.orders_rejected() == 6, "six orders should be rejected");
  Expect(
      risk.rejected_invalid_symbol() == 1,
      "invalid-symbol counter should be one");
  Expect(
      risk.rejected_invalid_price() == 1,
      "invalid-price counter should be one");
  Expect(
      risk.rejected_invalid_quantity() == 1,
      "invalid-quantity counter should be one");
  Expect(
      risk.rejected_quantity_limit() == 1,
      "quantity-limit counter should be one");
  Expect(
      risk.rejected_notional_limit() == 1,
      "notional-limit counter should be one");
  Expect(
      risk.rejected_symbol_not_allowed() == 1,
      "symbol-allowlist counter should be one");

  Expect(
      std::string_view(llgw::ToString(
          llgw::RiskRejectReason::kNotionalLimit)) ==
          "notional_limit",
      "reject reason should have a stable string representation");
}

void TestEmptyAllowlistAllowsAnyNonEmptySymbol() {
  llgw::RiskLimits limits;
  limits.max_order_quantity = 10;
  limits.max_order_notional = 2'000.0;

  llgw::PreTradeRiskManager risk(limits);

  const llgw::RiskDecision decision =
      risk.Check(MakeOrder("NVDA", 100.0, 10));

  Expect(
      decision.accepted,
      "empty allowlist should allow any otherwise-valid symbol");
}

}  // namespace

int main() {
  TestValidOrderPasses();
  TestExplicitRejectReasonsAndCounters();
  TestEmptyAllowlistAllowsAnyNonEmptySymbol();

  std::cout << "All pre-trade risk tests passed.\n";
  return 0;
}
