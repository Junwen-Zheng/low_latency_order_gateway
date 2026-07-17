#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "llgw/exchange_simulator.hpp"
#include "llgw/feed_replay.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_pipeline.hpp"
#include "llgw/pre_trade_risk.hpp"
#include "llgw/simple_strategy.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

std::filesystem::path WriteTempFeedFile(
    const std::string& filename,
    const std::string& contents) {
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / filename;

  std::ofstream output(path);
  Expect(output.is_open(), "temp feed file should open for writing");
  output << contents;
  output.close();

  return path;
}

void TestRiskLayerFiltersOrdersBeforeGateway() {
  const std::filesystem::path path = WriteTempFeedFile(
      "llgw_end_to_end_risk_feed.txt",
      "AAPL,100.00,100,100.10,200,1710000000123456789\n"
      "NVDA,100.00,100,100.10,200,1710000001123456789\n"
      "MSFT,300.00,100,300.10,200,1710000002123456789\n");

  llgw::RiskLimits limits;
  limits.max_order_quantity = 10;
  limits.max_order_notional = 2'000.0;
  limits.allowed_symbols[0] = "AAPL";
  limits.allowed_symbols[1] = "MSFT";
  limits.allowed_symbol_count = 2;

  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::PreTradeRiskManager risk(limits);
  llgw::OrderPipeline<8> pipeline(&gateway, &risk);
  llgw::SimpleStrategy strategy(0.05);

  const llgw::FeedReplayResult replay_result =
      llgw::ReplayMarketDataFile(
          path.string(),
          [&](const llgw::MarketDataUpdate& update) {
            const std::optional<llgw::OrderRequest> maybe_order =
                strategy.OnMarketData(update);

            if (maybe_order.has_value()) {
              Expect(
                  pipeline.Enqueue(*maybe_order),
                  "risk test order should enqueue");
            }
          });

  const std::size_t drained = pipeline.Drain();

  Expect(
      replay_result.status == llgw::FeedReplayStatus::kOk,
      "risk feed replay should succeed");
  Expect(
      strategy.orders_generated() == 3,
      "strategy should generate three orders");
  Expect(drained == 3, "pipeline should drain three orders");

  Expect(
      risk.orders_checked() == 3,
      "risk layer should check all three orders");
  Expect(
      risk.orders_accepted() == 1,
      "risk layer should accept only AAPL");
  Expect(
      risk.orders_rejected() == 2,
      "risk layer should reject two orders");
  Expect(
      risk.rejected_symbol_not_allowed() == 1,
      "NVDA should be rejected by the symbol allowlist");
  Expect(
      risk.rejected_notional_limit() == 1,
      "MSFT should be rejected by the notional limit");

  Expect(
      pipeline.orders_accepted() == 1,
      "pipeline should accept one order");
  Expect(
      pipeline.orders_rejected() == 2,
      "pipeline should reject two orders");
  Expect(
      pipeline.orders_rejected_by_risk() == 2,
      "pipeline should attribute two rejects to risk");
  Expect(
      pipeline.orders_rejected_by_gateway() == 0,
      "gateway should not reject an order in this test");

  Expect(
      gateway.orders_sent() == 1,
      "only the risk-approved order should reach the gateway");
  Expect(
      gateway.orders_accepted() == 1,
      "gateway should accept the approved order");
  Expect(
      gateway.orders_rejected() == 0,
      "gateway should receive no invalid orders");

  std::filesystem::remove(path);
}

}  // namespace

int main() {
  TestRiskLayerFiltersOrdersBeforeGateway();

  std::cout << "All end-to-end risk tests passed.\n";
  return 0;
}
