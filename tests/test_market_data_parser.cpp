#include <cstdlib>
#include <iostream>
#include <string_view>

#include "llgw/market_data_parser.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

void TestValidLine() {
  constexpr std::string_view line = "AAPL,192.10,100,192.12,200,1710000000123456789";

  const llgw::ParseResult result = llgw::ParseMarketDataLine(line);

  Expect(result.status == llgw::ParseStatus::kOk, "valid line should parse");
  Expect(result.update.symbol == "AAPL", "symbol should be AAPL");
  Expect(result.update.bid_price == 192.10, "bid price should match");
  Expect(result.update.bid_size == 100, "bid size should match");
  Expect(result.update.ask_price == 192.12, "ask price should match");
  Expect(result.update.ask_size == 200, "ask size should match");
  Expect(result.update.exchange_ts_ns == 1710000000123456789ULL,
         "exchange timestamp should match");
}

void TestEmptyLine() {
  const llgw::ParseResult result = llgw::ParseMarketDataLine("");
  Expect(result.status == llgw::ParseStatus::kEmptyInput, "empty line should fail");
}

void TestMissingFields() {
  const llgw::ParseResult result = llgw::ParseMarketDataLine("AAPL,192.10,100");
  Expect(result.status == llgw::ParseStatus::kInvalidNumber ||
             result.status == llgw::ParseStatus::kWrongFieldCount,
         "missing fields should fail");
}

void TestInvalidNumber() {
  const llgw::ParseResult result =
      llgw::ParseMarketDataLine("AAPL,abc,100,192.12,200,1710000000123456789");
  Expect(result.status == llgw::ParseStatus::kInvalidNumber, "invalid number should fail");
}

void TestCrossedMarket() {
  const llgw::ParseResult result =
      llgw::ParseMarketDataLine("AAPL,193.00,100,192.00,200,1710000000123456789");
  Expect(result.status == llgw::ParseStatus::kInvalidMarket, "crossed market should fail");
}

}  // namespace

int main() {
  TestValidLine();
  TestEmptyLine();
  TestMissingFields();
  TestInvalidNumber();
  TestCrossedMarket();

  std::cout << "All parser tests passed.\n";
  return 0;
}
