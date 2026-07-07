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

void TestEndToEndPipelineFlow() {
  const std::filesystem::path path = WriteTempFeedFile(
      "llgw_end_to_end_pipeline_feed.txt",
      "AAPL,100.00,100,100.10,200,1710000000123456789\n"
      "MSFT,200.00,100,200.01,200,1710000001123456789\n"
      "NVDA,300.00,100,300.20,200,1710000002123456789\n");

  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderPipeline<8> pipeline(&gateway);
  llgw::SimpleStrategy strategy(0.05);

  const llgw::FeedReplayResult replay_result = llgw::ReplayMarketDataFile(
      path.string(), [&](const llgw::MarketDataUpdate& update) {
        const std::optional<llgw::OrderRequest> maybe_order = strategy.OnMarketData(update);
        if (!maybe_order.has_value()) {
          return;
        }

        Expect(pipeline.Enqueue(*maybe_order), "pipeline enqueue should succeed");
      });

  pipeline.Drain();

  Expect(replay_result.status == llgw::FeedReplayStatus::kOk, "feed replay should succeed");
  Expect(replay_result.lines_read == 3, "replay should read 3 lines");
  Expect(replay_result.updates_parsed == 3, "replay should parse 3 updates");

  Expect(strategy.signals_seen() == 3, "strategy should see 3 updates");
  Expect(strategy.orders_generated() == 2, "strategy should generate 2 orders");

  Expect(pipeline.orders_enqueued() == 2, "pipeline should enqueue 2 orders");
  Expect(pipeline.orders_dropped_on_enqueue() == 0, "pipeline should drop 0 orders");
  Expect(pipeline.orders_drained() == 2, "pipeline should drain 2 orders");
  Expect(pipeline.orders_accepted() == 2, "pipeline should accept 2 orders");
  Expect(pipeline.orders_rejected() == 0, "pipeline should reject 0 orders");

  Expect(gateway.orders_sent() == 2, "gateway should send 2 orders");
  Expect(gateway.orders_accepted() == 2, "gateway should accept 2 orders");
  Expect(gateway.orders_rejected() == 0, "gateway should reject 0 orders");

  std::filesystem::remove(path);
}

void TestEndToEndPipelineBackpressure() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderPipeline<2> pipeline(&gateway);

  llgw::OrderRequest first{};
  first.sequence_id = 1;
  first.symbol = "AAPL";
  first.side = llgw::Side::kBuy;
  first.price = 100.10;
  first.quantity = 10;

  llgw::OrderRequest second = first;
  second.sequence_id = 2;

  llgw::OrderRequest third = first;
  third.sequence_id = 3;

  Expect(pipeline.Enqueue(first), "first enqueue should succeed");
  Expect(pipeline.Enqueue(second), "second enqueue should succeed");
  Expect(!pipeline.Enqueue(third), "third enqueue should fail due to full buffer");

  Expect(pipeline.orders_enqueued() == 2, "pipeline should enqueue 2 orders");
  Expect(pipeline.orders_dropped_on_enqueue() == 1, "pipeline should drop 1 order");

  const std::size_t drained = pipeline.Drain();

  Expect(drained == 2, "pipeline should drain 2 orders");
  Expect(gateway.orders_sent() == 2, "gateway should send only 2 orders");
  Expect(gateway.orders_accepted() == 2, "gateway should accept 2 orders");
  Expect(gateway.orders_rejected() == 0, "gateway should reject 0 orders");
}

}  // namespace

int main() {
  TestEndToEndPipelineFlow();
  TestEndToEndPipelineBackpressure();

  std::cout << "All end-to-end pipeline tests passed.\n";
  return 0;
}
