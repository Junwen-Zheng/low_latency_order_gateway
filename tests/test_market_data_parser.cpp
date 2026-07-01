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

void ExpectStatus(std::string_view line, llgw::ParseStatus expected_status, const char* message) {
  const llgw::ParseResult result = llgw::ParseMarketDataLine(line);
  Expect(result.status == expected_status, message);
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

void TestValidLineWithCarriageReturn() {
  constexpr std::string_view line = "AAPL,192.10,100,192.12,200,1710000000123456789\r";

  const llgw::ParseResult result = llgw::ParseMarketDataLine(line);

  Expect(result.status == llgw::ParseStatus::kOk, "CRLF-style line should parse");
  Expect(result.update.symbol == "AAPL", "symbol should be AAPL after CR stripping");
}

void TestEmptyLine() {
  ExpectStatus("", llgw::ParseStatus::kEmptyInput, "empty line should fail");
}

void TestMissingFields() {
  ExpectStatus("AAPL,192.10,100", llgw::ParseStatus::kWrongFieldCount,
               "missing fields should fail with wrong field count");
}

void TestExtraFields() {
  ExpectStatus("AAPL,192.10,100,192.12,200,1710000000123456789,EXTRA",
               llgw::ParseStatus::kWrongFieldCount,
               "extra fields should fail with wrong field count");
}

void TestEmptySymbol() {
  ExpectStatus(",192.10,100,192.12,200,1710000000123456789",
               llgw::ParseStatus::kWrongFieldCount, "empty symbol should fail");
}

void TestInvalidBidPrice() {
  ExpectStatus("AAPL,abc,100,192.12,200,1710000000123456789",
               llgw::ParseStatus::kInvalidNumber, "invalid bid price should fail");
}

void TestInvalidBidSize() {
  ExpectStatus("AAPL,192.10,abc,192.12,200,1710000000123456789",
               llgw::ParseStatus::kInvalidNumber, "invalid bid size should fail");
}

void TestInvalidAskPrice() {
  ExpectStatus("AAPL,192.10,100,abc,200,1710000000123456789",
               llgw::ParseStatus::kInvalidNumber, "invalid ask price should fail");
}

void TestInvalidAskSize() {
  ExpectStatus("AAPL,192.10,100,192.12,abc,1710000000123456789",
               llgw::ParseStatus::kInvalidNumber, "invalid ask size should fail");
}

void TestInvalidTimestamp() {
  ExpectStatus("AAPL,192.10,100,192.12,200,abc", llgw::ParseStatus::kInvalidNumber,
               "invalid timestamp should fail");
}

void TestNegativeBidPrice() {
  ExpectStatus("AAPL,-192.10,100,192.12,200,1710000000123456789",
               llgw::ParseStatus::kInvalidMarket, "negative bid price should fail");
}

void TestZeroBidSize() {
  ExpectStatus("AAPL,192.10,0,192.12,200,1710000000123456789",
               llgw::ParseStatus::kInvalidMarket, "zero bid size should fail");
}

void TestZeroAskSize() {
  ExpectStatus("AAPL,192.10,100,192.12,0,1710000000123456789",
               llgw::ParseStatus::kInvalidMarket, "zero ask size should fail");
}

void TestCrossedMarket() {
  ExpectStatus("AAPL,193.00,100,192.00,200,1710000000123456789",
               llgw::ParseStatus::kInvalidMarket, "crossed market should fail");
}

void TestLockedMarketIsAllowed() {
  const llgw::ParseResult result =
      llgw::ParseMarketDataLine("AAPL,192.10,100,192.10,200,1710000000123456789");

  Expect(result.status == llgw::ParseStatus::kOk, "locked market should be allowed for now");
  Expect(result.update.bid_price == result.update.ask_price, "bid and ask should match");
}

}  // namespace

int main() {
  TestValidLine();
  TestValidLineWithCarriageReturn();
  TestEmptyLine();
  TestMissingFields();
  TestExtraFields();
  TestEmptySymbol();
  TestInvalidBidPrice();
  TestInvalidBidSize();
  TestInvalidAskPrice();
  TestInvalidAskSize();
  TestInvalidTimestamp();
  TestNegativeBidPrice();
  TestZeroBidSize();
  TestZeroAskSize();
  TestCrossedMarket();
  TestLockedMarketIsAllowed();

  std::cout << "All parser tests passed.\n";
  return 0;
}
