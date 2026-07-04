#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "llgw/exchange_simulator.hpp"
#include "llgw/feed_replay.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/simple_strategy.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

std::filesystem::path WriteTempFeedFile(const std::string& filename, const std::string& contents) {
  const std::filesystem::path path = std::filesystem::temp_directory_path() / filename;

  std::ofstream output(path);
  Expect(output.is_open(), "temp feed file should open for writing");
  output << contents;
  output.close();

  return path;
}

void TestEndToEndOrderFlow() {
  const std::filesystem::path path = WriteTempFeedFile(
      "llgw_end_to_end_feed.txt",
      "AAPL,100.00,100,100.01,200,1710000000123456789\n"
      "MSFT,200.00,100,200.10,200,1710000001123456789\n"
      "NVDA,300.00,100,300.20,200,1710000002123456789\n"
      "GOOG,400.00,100,400.01,200,1710000003123456789\n");

  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::SimpleStrategy strategy(0.05);

  const llgw::FeedReplayResult replay_result = llgw::ReplayMarketDataFile(
      path.string(), [&](const llgw::MarketDataUpdate& update) {
        const std::optional<llgw::OrderRequest> maybe_order = strategy.OnMarketData(update);
        if (!maybe_order.has_value()) {
          return;
        }

        const llgw::OrderResponse response = gateway.SendOrder(*maybe_order);
        Expect(response.accepted, "generated strategy order should be accepted");
      });

  Expect(replay_result.status == llgw::FeedReplayStatus::kOk, "feed replay should succeed");
  Expect(replay_result.lines_read == 4, "replay should read 4 lines");
  Expect(replay_result.updates_parsed == 4, "replay should parse 4 updates");

  Expect(strategy.signals_seen() == 4, "strategy should see 4 updates");
  Expect(strategy.orders_generated() == 2, "strategy should generate 2 orders");

  Expect(gateway.orders_sent() == 2, "gateway should send 2 orders");
  Expect(gateway.orders_accepted() == 2, "gateway should accept 2 orders");
  Expect(gateway.orders_rejected() == 0, "gateway should reject 0 orders");

  std::filesystem::remove(path);
}

void TestEndToEndStopsOnParseErrorBeforeLaterOrders() {
  const std::filesystem::path path = WriteTempFeedFile(
      "llgw_end_to_end_bad_feed.txt",
      "AAPL,100.00,100,100.10,200,1710000000123456789\n"
      "MSFT,bad-price,100,200.10,200,1710000001123456789\n"
      "NVDA,300.00,100,300.20,200,1710000002123456789\n");

  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::SimpleStrategy strategy(0.05);

  const llgw::FeedReplayResult replay_result = llgw::ReplayMarketDataFile(
      path.string(), [&](const llgw::MarketDataUpdate& update) {
        const std::optional<llgw::OrderRequest> maybe_order = strategy.OnMarketData(update);
        if (maybe_order.has_value()) {
          const llgw::OrderResponse response = gateway.SendOrder(*maybe_order);
          Expect(response.accepted, "first generated order should be accepted");
        }
      });

  Expect(replay_result.status == llgw::FeedReplayStatus::kParseError,
         "bad feed should stop with parse error");
  Expect(replay_result.lines_read == 2, "replay should stop at bad line 2");
  Expect(replay_result.updates_parsed == 1, "only first update should parse");

  Expect(strategy.signals_seen() == 1, "strategy should only see first valid update");
  Expect(strategy.orders_generated() == 1, "strategy should only generate one order");

  Expect(gateway.orders_sent() == 1, "gateway should only send one order");
  Expect(gateway.orders_accepted() == 1, "gateway should accept one order");
  Expect(gateway.orders_rejected() == 0, "gateway should reject zero orders");

  std::filesystem::remove(path);
}

}  // namespace

int main() {
  TestEndToEndOrderFlow();
  TestEndToEndStopsOnParseErrorBeforeLaterOrders();

  std::cout << "All end-to-end order flow tests passed.\n";
  return 0;
}
