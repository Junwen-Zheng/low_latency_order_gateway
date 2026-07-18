#include <cstdlib>
#include <iostream>
#include <string_view>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_lifecycle.hpp"
#include "llgw/order_pipeline.hpp"
#include "llgw/pre_trade_risk.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::OrderRequest MakeOrder(
    std::uint64_t sequence_id,
    std::string_view symbol,
    double price,
    std::uint32_t quantity) {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = symbol;
  request.side = llgw::Side::kBuy;
  request.price = price;
  request.quantity = quantity;
  return request;
}

void TestAcceptedAndRiskRejectedOrders() {
  llgw::RiskLimits limits;
  limits.max_order_quantity = 100;
  limits.max_order_notional = 100'000.0;
  limits.allowed_symbols[0] = "AAPL";
  limits.allowed_symbol_count = 1;

  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::PreTradeRiskManager risk(limits);
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderPipeline<8> pipeline(&gateway, &risk, &lifecycle);

  Expect(
      pipeline.Enqueue(MakeOrder(1, "AAPL", 100.0, 10)),
      "allowed order should enqueue");
  Expect(
      pipeline.Enqueue(MakeOrder(2, "NVDA", 100.0, 10)),
      "risk-rejected order should enqueue before drain");

  Expect(pipeline.Drain() == 2, "two orders should drain");
  Expect(
      gateway.orders_sent() == 1,
      "risk-rejected order must not reach gateway");

  const auto accepted = lifecycle.Find(1);
  const auto risk_rejected = lifecycle.Find(2);

  Expect(accepted.has_value(), "accepted order should be tracked");
  Expect(
      accepted->state ==
          llgw::OrderLifecycleState::kExchangeAccepted,
      "allowed order should reach accepted state");

  Expect(risk_rejected.has_value(), "risk reject should be tracked");
  Expect(
      risk_rejected->state ==
          llgw::OrderLifecycleState::kRiskRejected,
      "disallowed order should stop at risk rejection");
  Expect(
      risk_rejected->risk_reject_reason ==
          llgw::RiskRejectReason::kSymbolNotAllowed,
      "risk reason should be preserved");
  Expect(
      lifecycle.orders_sent() == 1,
      "only approved order should enter sent state");
  Expect(
      pipeline.lifecycle_transition_errors() == 0,
      "valid flow should have no lifecycle errors");
}

void TestExchangeRejectedOrder() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderPipeline<4> pipeline(&gateway, nullptr, &lifecycle);

  Expect(
      pipeline.Enqueue(MakeOrder(10, "AAPL", 100.0, 0)),
      "invalid exchange order should enqueue without risk");

  pipeline.Drain();

  const auto rejected = lifecycle.Find(10);
  Expect(rejected.has_value(), "exchange reject should be tracked");
  Expect(
      rejected->state ==
          llgw::OrderLifecycleState::kExchangeRejected,
      "invalid order should reach exchange-rejected state");
  Expect(
      rejected->exchange_reject_reason ==
          llgw::OrderRejectReason::kInvalidQuantity,
      "exchange reject reason should be preserved");
}

void TestDuplicateLifecycleRegistrationStopsBeforeGateway() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderPipeline<4> pipeline(&gateway, nullptr, &lifecycle);

  const llgw::OrderRequest order =
      MakeOrder(20, "AAPL", 100.0, 10);

  Expect(pipeline.Enqueue(order), "first order should enqueue");
  Expect(
      !pipeline.Enqueue(order),
      "duplicate lifecycle registration should be rejected");

  pipeline.Drain();

  Expect(
      pipeline.orders_rejected_by_lifecycle_on_enqueue() == 1,
      "pipeline should count lifecycle duplicate rejection");
  Expect(
      gateway.orders_sent() == 1,
      "duplicate order should not reach gateway twice");
  Expect(
      lifecycle.orders_registered() == 1,
      "only one lifecycle record should exist");
}

}  // namespace

int main() {
  TestAcceptedAndRiskRejectedOrders();
  TestExchangeRejectedOrder();
  TestDuplicateLifecycleRegistrationStopsBeforeGateway();

  std::cout << "All end-to-end lifecycle tests passed.\n";
  return 0;
}
