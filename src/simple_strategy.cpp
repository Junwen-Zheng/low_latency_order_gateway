#include "llgw/simple_strategy.hpp"

namespace llgw {
namespace {

constexpr double kPriceEpsilon = 1e-9;

}  // namespace

SimpleStrategy::SimpleStrategy(double min_spread) : min_spread_(min_spread) {}

std::optional<OrderRequest> SimpleStrategy::OnMarketData(const MarketDataUpdate& update) {
  ++signals_seen_;

  const double spread = update.ask_price - update.bid_price;
  if (spread + kPriceEpsilon < min_spread_) {
    return std::nullopt;
  }

  OrderRequest request{};
  request.sequence_id = next_sequence_id_++;

  if (!request.symbol.Assign(update.symbol)) {
    return std::nullopt;
  }

  request.side = Side::kBuy;
  request.price = update.ask_price;
  request.quantity = 10;

  ++orders_generated_;
  return request;
}

}  // namespace llgw
