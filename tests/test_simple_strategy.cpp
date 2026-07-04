#include <cstdlib>
#include <iostream>

#include "llgw/messages.hpp"
#include "llgw/simple_strategy.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::MarketDataUpdate MakeUpdate(double bid, double ask) {
  llgw::MarketDataUpdate update{};
  update.symbol = "AAPL";
  update.bid_price = bid;
  update.bid_size = 100;
  update.ask_price = ask;
  update.ask_size = 200;
  update.exchange_ts_ns = 1710000000123456789ULL;
  return update;
}

void TestNoOrderWhenSpreadBelowThreshold() {
  llgw::SimpleStrategy strategy(0.05);

  const std::optional<llgw::OrderRequest> order = strategy.OnMarketData(MakeUpdate(100.00, 100.02));

  Expect(!order.has_value(), "strategy should not generate order below threshold");
  Expect(strategy.signals_seen() == 1, "signals seen should be 1");
  Expect(strategy.orders_generated() == 0, "orders generated should be 0");
}

void TestOrderGeneratedWhenSpreadMeetsThreshold() {
  llgw::SimpleStrategy strategy(0.05);

  const std::optional<llgw::OrderRequest> order = strategy.OnMarketData(MakeUpdate(100.00, 100.05));

  Expect(order.has_value(), "strategy should generate order when spread meets threshold");
  Expect(order->sequence_id == 1, "first sequence id should be 1");
  Expect(order->symbol == "AAPL", "symbol should match update");
  Expect(order->side == llgw::Side::kBuy, "side should be buy");
  Expect(order->price == 100.05, "order price should use ask price");
  Expect(order->quantity == 10, "order quantity should be fixed test quantity");
  Expect(strategy.signals_seen() == 1, "signals seen should be 1");
  Expect(strategy.orders_generated() == 1, "orders generated should be 1");
}

void TestSequenceIdsIncrease() {
  llgw::SimpleStrategy strategy(0.01);

  const std::optional<llgw::OrderRequest> first = strategy.OnMarketData(MakeUpdate(100.00, 100.02));
  const std::optional<llgw::OrderRequest> second = strategy.OnMarketData(MakeUpdate(101.00, 101.02));

  Expect(first.has_value(), "first order should be generated");
  Expect(second.has_value(), "second order should be generated");
  Expect(first->sequence_id == 1, "first sequence id should be 1");
  Expect(second->sequence_id == 2, "second sequence id should be 2");
  Expect(strategy.signals_seen() == 2, "signals seen should be 2");
  Expect(strategy.orders_generated() == 2, "orders generated should be 2");
}

}  // namespace

int main() {
  TestNoOrderWhenSpreadBelowThreshold();
  TestOrderGeneratedWhenSpreadMeetsThreshold();
  TestSequenceIdsIncrease();

  std::cout << "All simple strategy tests passed.\n";
  return 0;
}
