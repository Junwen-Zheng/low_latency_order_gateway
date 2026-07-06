#include <iostream>
#include <optional>

#include "llgw/exchange_simulator.hpp"
#include "llgw/feed_replay.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_pipeline.hpp"
#include "llgw/simple_strategy.hpp"

int main() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderPipeline<16> pipeline(&gateway);
  llgw::SimpleStrategy strategy(0.02);

  const llgw::FeedReplayResult replay_result = llgw::ReplayMarketDataFile(
      "data/sample_feed.txt", [&](const llgw::MarketDataUpdate& update) {
        const std::optional<llgw::OrderRequest> maybe_order = strategy.OnMarketData(update);
        if (!maybe_order.has_value()) {
          return;
        }

        if (!pipeline.Enqueue(*maybe_order)) {
          std::cerr << "Pipeline dropped order due to full buffer\n";
          return;
        }

        // Drain immediately because OrderRequest currently stores symbol as std::string_view.
        // This keeps the symbol view within the lifetime of the current replay line.
        pipeline.Drain();
      });

  if (replay_result.status != llgw::FeedReplayStatus::kOk) {
    std::cerr << "Feed replay failed: " << llgw::ToString(replay_result.status) << "\n";
    if (replay_result.status == llgw::FeedReplayStatus::kParseError) {
      std::cerr << "  error_line=" << replay_result.error_line << "\n";
    }
    return 1;
  }

  std::cout << "Replay completed successfully\n";
  std::cout << "  lines_read=" << replay_result.lines_read << "\n";
  std::cout << "  updates_parsed=" << replay_result.updates_parsed << "\n";
  std::cout << "Strategy summary\n";
  std::cout << "  signals_seen=" << strategy.signals_seen() << "\n";
  std::cout << "  orders_generated=" << strategy.orders_generated() << "\n";
  std::cout << "Order pipeline summary\n";
  std::cout << "  orders_enqueued=" << pipeline.orders_enqueued() << "\n";
  std::cout << "  orders_dropped_on_enqueue=" << pipeline.orders_dropped_on_enqueue() << "\n";
  std::cout << "  orders_drained=" << pipeline.orders_drained() << "\n";
  std::cout << "  orders_accepted=" << pipeline.orders_accepted() << "\n";
  std::cout << "  orders_rejected=" << pipeline.orders_rejected() << "\n";
  std::cout << "Order gateway summary\n";
  std::cout << "  orders_sent=" << gateway.orders_sent() << "\n";
  std::cout << "  orders_accepted=" << gateway.orders_accepted() << "\n";
  std::cout << "  orders_rejected=" << gateway.orders_rejected() << "\n";

  return 0;
}
