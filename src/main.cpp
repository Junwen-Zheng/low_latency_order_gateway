#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "llgw/market_data_parser.hpp"

int main() {
  const char* sample_feed_path = "data/sample_feed.txt";

  std::ifstream input(sample_feed_path);
  if (!input.is_open()) {
    std::cerr << "Failed to open sample feed file: " << sample_feed_path << "\n";
    return 1;
  }

  std::size_t parsed_count = 0;
  std::string line;

  while (std::getline(input, line)) {
    const llgw::ParseResult result = llgw::ParseMarketDataLine(line);
    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Failed to parse sample feed line: " << line << "\n";
      return 1;
    }

    ++parsed_count;
  }

  std::cout << "Parsed sample feed updates: " << parsed_count << "\n";
  return 0;
}
