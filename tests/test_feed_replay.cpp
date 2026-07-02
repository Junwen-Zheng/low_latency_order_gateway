#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "llgw/feed_replay.hpp"
#include "llgw/messages.hpp"

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

void TestReplayValidFeed() {
  const std::filesystem::path path = WriteTempFeedFile(
      "llgw_valid_feed.txt",
      "AAPL,192.10,100,192.12,200,1710000000123456789\n"
      "MSFT,420.50,150,420.55,250,1710000001123456789\n"
      "NVDA,880.10,75,880.20,125,1710000002123456789\n");

  std::vector<std::string> symbols;
  std::vector<std::uint64_t> timestamps;

  const llgw::FeedReplayResult result = llgw::ReplayMarketDataFile(
      path.string(), [&](const llgw::MarketDataUpdate& update) {
        symbols.emplace_back(update.symbol);
        timestamps.push_back(update.exchange_ts_ns);
      });

  Expect(result.status == llgw::FeedReplayStatus::kOk, "valid feed replay should succeed");
  Expect(result.lines_read == 3, "valid feed should read 3 lines");
  Expect(result.updates_parsed == 3, "valid feed should parse 3 updates");
  Expect(symbols.size() == 3, "callback should be invoked 3 times");
  Expect(symbols[0] == "AAPL", "first symbol should be AAPL");
  Expect(symbols[1] == "MSFT", "second symbol should be MSFT");
  Expect(symbols[2] == "NVDA", "third symbol should be NVDA");
  Expect(timestamps[0] == 1710000000123456789ULL, "first timestamp should match");
  Expect(timestamps[2] == 1710000002123456789ULL, "third timestamp should match");

  std::filesystem::remove(path);
}

void TestReplayStopsAtFirstBadLine() {
  const std::filesystem::path path = WriteTempFeedFile(
      "llgw_bad_feed.txt",
      "AAPL,192.10,100,192.12,200,1710000000123456789\n"
      "MSFT,not-a-price,150,420.55,250,1710000001123456789\n"
      "NVDA,880.10,75,880.20,125,1710000002123456789\n");

  std::size_t callback_count = 0;

  const llgw::FeedReplayResult result = llgw::ReplayMarketDataFile(
      path.string(), [&](const llgw::MarketDataUpdate&) { ++callback_count; });

  Expect(result.status == llgw::FeedReplayStatus::kParseError,
         "bad feed replay should stop with parse error");
  Expect(result.lines_read == 2, "bad feed should stop on line 2");
  Expect(result.updates_parsed == 1, "only first update should parse");
  Expect(result.error_line == 2, "error line should be 2");
  Expect(result.parse_status == llgw::ParseStatus::kInvalidNumber,
         "parse status should preserve invalid number error");
  Expect(callback_count == 1, "callback should only be invoked for valid lines before error");

  std::filesystem::remove(path);
}

void TestReplayMissingFile() {
  const llgw::FeedReplayResult result =
      llgw::ReplayMarketDataFile("/tmp/llgw_file_that_should_not_exist.txt",
                                 [](const llgw::MarketDataUpdate&) {});

  Expect(result.status == llgw::FeedReplayStatus::kFileOpenFailed,
         "missing file should return file open failure");
  Expect(result.lines_read == 0, "missing file should read zero lines");
  Expect(result.updates_parsed == 0, "missing file should parse zero updates");
}

void TestReplayEmptyFile() {
  const std::filesystem::path path = WriteTempFeedFile("llgw_empty_feed.txt", "");

  const llgw::FeedReplayResult result =
      llgw::ReplayMarketDataFile(path.string(), [](const llgw::MarketDataUpdate&) {});

  Expect(result.status == llgw::FeedReplayStatus::kOk, "empty file should replay successfully");
  Expect(result.lines_read == 0, "empty file should read zero lines");
  Expect(result.updates_parsed == 0, "empty file should parse zero updates");

  std::filesystem::remove(path);
}

}  // namespace

int main() {
  TestReplayValidFeed();
  TestReplayStopsAtFirstBadLine();
  TestReplayMissingFile();
  TestReplayEmptyFile();

  std::cout << "All feed replay tests passed.\n";
  return 0;
}
