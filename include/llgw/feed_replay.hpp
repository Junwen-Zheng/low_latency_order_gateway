#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

#include "llgw/market_data_parser.hpp"
#include "llgw/messages.hpp"

namespace llgw {

enum class FeedReplayStatus {
  kOk,
  kFileOpenFailed,
  kParseError,
};

struct FeedReplayResult {
  FeedReplayStatus status = FeedReplayStatus::kFileOpenFailed;

  std::size_t lines_read = 0;
  std::size_t updates_parsed = 0;

  std::size_t error_line = 0;
  ParseStatus parse_status = ParseStatus::kOk;
};

using MarketDataCallback = std::function<void(const MarketDataUpdate&)>;

FeedReplayResult ReplayMarketDataFile(const std::string& path, const MarketDataCallback& on_update);

std::string_view ToString(FeedReplayStatus status);

}  // namespace llgw
