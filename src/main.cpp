#include <iostream>
#include <string_view>

#include "llgw/market_data_parser.hpp"

int main() {
  constexpr std::string_view line = "AAPL,192.10,100,192.12,200,1710000000123456789";

  const llgw::ParseResult result = llgw::ParseMarketDataLine(line);

  if (result.status != llgw::ParseStatus::kOk) {
    std::cerr << "Failed to parse sample market data line\n";
    return 1;
  }

  std::cout << "Parsed market data update:\n";
  std::cout << "  symbol=" << result.update.symbol << "\n";
  std::cout << "  bid=" << result.update.bid_price << " x " << result.update.bid_size << "\n";
  std::cout << "  ask=" << result.update.ask_price << " x " << result.update.ask_size << "\n";
  std::cout << "  exchange_ts_ns=" << result.update.exchange_ts_ns << "\n";

  return 0;
}
