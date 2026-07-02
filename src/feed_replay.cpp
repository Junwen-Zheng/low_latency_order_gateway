#include "llgw/feed_replay.hpp"

#include <fstream>
#include <string>
#include <string_view>

namespace llgw {

FeedReplayResult ReplayMarketDataFile(const std::string& path, const MarketDataCallback& on_update) {
  FeedReplayResult result{};

  std::ifstream input(path);
  if (!input.is_open()) {
    result.status = FeedReplayStatus::kFileOpenFailed;
    return result;
  }

  std::string line;
  while (std::getline(input, line)) {
    ++result.lines_read;

    const ParseResult parse_result = ParseMarketDataLine(line);
    if (parse_result.status != ParseStatus::kOk) {
      result.status = FeedReplayStatus::kParseError;
      result.error_line = result.lines_read;
      result.parse_status = parse_result.status;
      return result;
    }

    if (on_update) {
      on_update(parse_result.update);
    }

    ++result.updates_parsed;
  }

  result.status = FeedReplayStatus::kOk;
  return result;
}

std::string_view ToString(FeedReplayStatus status) {
  switch (status) {
    case FeedReplayStatus::kOk:
      return "ok";
    case FeedReplayStatus::kFileOpenFailed:
      return "file_open_failed";
    case FeedReplayStatus::kParseError:
      return "parse_error";
  }

  return "unknown";
}

}  // namespace llgw
