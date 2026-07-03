#include <iostream>

#include "llgw/exchange_simulator.hpp"
#include "llgw/feed_replay.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"

int main() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);

  std::uint64_t next_order_sequence_id = 1;

  const llgw::FeedReplayResult replay_result = llgw::ReplayMarketDataFile(
      "data/sample_feed.txt", [&](const llgw::MarketDataUpdate& update) {
        if (update.symbol == "AAPL") {
          llgw::OrderRequest request{};
          request.sequence_id = next_order_sequence_id++;
          request.symbol = update.symbol;
          request.side = llgw::Side::kBuy;
          request.price = update.ask_price;
          request.quantity = 10;

          const llgw::OrderResponse response = gateway.SendOrder(request);
          if (!response.accepted) {
            std::cerr << "Order rejected for sequence_id=" << response.sequence_id << "\n";
          }
        }
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
  std::cout << "Order gateway summary\n";
  std::cout << "  orders_sent=" << gateway.orders_sent() << "\n";
  std::cout << "  orders_accepted=" << gateway.orders_accepted() << "\n";
  std::cout << "  orders_rejected=" << gateway.orders_rejected() << "\n";

  return 0;
}
