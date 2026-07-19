#include <iostream>
#include <optional>

#include "llgw/exchange_simulator.hpp"
#include "llgw/feed_replay.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_lifecycle.hpp"
#include "llgw/order_pipeline.hpp"
#include "llgw/pre_trade_risk.hpp"
#include "llgw/simple_strategy.hpp"

int main() {
  llgw::ExchangeSimulator exchange;
  llgw::RiskLimits risk_limits;
  risk_limits.max_order_quantity = 1000;
  risk_limits.max_order_notional = 1'000'000.0;
  risk_limits.allowed_symbols[0] = "AAPL";
  risk_limits.allowed_symbols[1] = "MSFT";
  risk_limits.allowed_symbols[2] = "NVDA";
  risk_limits.allowed_symbols[3] = "GOOG";
  risk_limits.allowed_symbols[4] = "TSLA";
  risk_limits.allowed_symbols[5] = "AMZN";
  risk_limits.allowed_symbols[6] = "META";
  risk_limits.allowed_symbols[7] = "SPY";
  risk_limits.allowed_symbol_count = 8;
  llgw::PreTradeRiskManager risk_manager(risk_limits);
  llgw::OrderLifecycleTracker lifecycle_tracker;
  llgw::OrderGateway gateway(&exchange, &lifecycle_tracker);
  llgw::OrderPipeline<16> pipeline(
      &gateway,
      &risk_manager,
      &lifecycle_tracker);
  llgw::SimpleStrategy strategy(0.02);

  const llgw::FeedReplayResult replay_result = llgw::ReplayMarketDataFile(
      "data/sample_feed.txt", [&](const llgw::MarketDataUpdate& update) {
        const std::optional<llgw::OrderRequest> maybe_order = strategy.OnMarketData(update);
        if (!maybe_order.has_value()) {
          return;
        }

        if (!pipeline.Enqueue(*maybe_order)) {
          std::cerr << "Pipeline dropped order due to full buffer\n";
        }
      });

  if (replay_result.status != llgw::FeedReplayStatus::kOk) {
    std::cerr << "Feed replay failed: " << llgw::ToString(replay_result.status) << "\n";
    if (replay_result.status == llgw::FeedReplayStatus::kParseError) {
      std::cerr << "  error_line=" << replay_result.error_line << "\n";
    }
    return 1;
  }

  pipeline.Drain();

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
  std::cout << "Order lifecycle summary\n";
  std::cout << "  orders_registered="
            << lifecycle_tracker.orders_registered() << "\n";
  std::cout << "  orders_queued="
            << lifecycle_tracker.orders_queued() << "\n";
  std::cout << "  orders_risk_rejected="
            << lifecycle_tracker.orders_risk_rejected() << "\n";
  std::cout << "  orders_sent="
            << lifecycle_tracker.orders_sent() << "\n";
  std::cout << "  orders_exchange_accepted="
            << lifecycle_tracker.orders_exchange_accepted() << "\n";
  std::cout << "  orders_exchange_rejected="
            << lifecycle_tracker.orders_exchange_rejected() << "\n";
  std::cout << "  transition_errors="
            << lifecycle_tracker.transition_errors() << "\n";

  return 0;
}
