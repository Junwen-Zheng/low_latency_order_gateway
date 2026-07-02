#include <iostream>

#include "llgw/feed_replay.hpp"

int main() {
  const llgw::FeedReplayResult result =
      llgw::ReplayMarketDataFile("data/sample_feed.txt", [](const llgw::MarketDataUpdate&) {});

  if (result.status != llgw::FeedReplayStatus::kOk) {
    std::cerr << "Feed replay failed: " << llgw::ToString(result.status) << "\n";
    if (result.status == llgw::FeedReplayStatus::kParseError) {
      std::cerr << "  error_line=" << result.error_line << "\n";
    }
    return 1;
  }

  std::cout << "Replay completed successfully\n";
  std::cout << "  lines_read=" << result.lines_read << "\n";
  std::cout << "  updates_parsed=" << result.updates_parsed << "\n";

  return 0;
}
