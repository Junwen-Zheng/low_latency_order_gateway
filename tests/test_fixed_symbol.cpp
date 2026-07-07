#include <cstdlib>
#include <iostream>
#include <string_view>

#include "llgw/fixed_symbol.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

void TestDefaultSymbolIsEmpty() {
  llgw::FixedSymbol symbol;

  Expect(symbol.empty(), "default symbol should be empty");
  Expect(symbol.size() == 0, "default symbol size should be zero");
  Expect(symbol.view().empty(), "default symbol view should be empty");
}

void TestAssignValidSymbol() {
  llgw::FixedSymbol symbol;

  Expect(symbol.Assign("AAPL"), "valid symbol should assign");
  Expect(!symbol.empty(), "assigned symbol should not be empty");
  Expect(symbol.size() == 4, "AAPL size should be 4");
  Expect(symbol.view() == "AAPL", "symbol view should be AAPL");
  Expect(symbol == "AAPL", "symbol equality should work");
  Expect(symbol != "MSFT", "symbol inequality should work");
}

void TestConstructorFromStringView() {
  llgw::FixedSymbol symbol("MSFT");

  Expect(!symbol.empty(), "constructed symbol should not be empty");
  Expect(symbol.view() == "MSFT", "constructed symbol should be MSFT");
}

void TestAssignmentOperator() {
  llgw::FixedSymbol symbol;

  symbol = "NVDA";

  Expect(symbol.view() == "NVDA", "assigned symbol should be NVDA");
}

void TestRejectEmptySymbol() {
  llgw::FixedSymbol symbol("AAPL");

  Expect(!symbol.Assign(""), "empty symbol should be rejected");
  Expect(symbol.empty(), "rejected empty symbol should reset to empty");
}

void TestRejectTooLongSymbol() {
  llgw::FixedSymbol symbol("AAPL");

  constexpr std::string_view too_long = "ABCDEFGHIJKLMNOPQ";

  Expect(too_long.size() == llgw::FixedSymbol::kMaxLength + 1,
         "test symbol should be one char too long");
  Expect(!symbol.Assign(too_long), "too-long symbol should be rejected");
  Expect(symbol.empty(), "rejected too-long symbol should reset to empty");
}

void TestMaxLengthSymbol() {
  llgw::FixedSymbol symbol;

  constexpr std::string_view max_length = "ABCDEFGHIJKLMNOP";

  Expect(max_length.size() == llgw::FixedSymbol::kMaxLength,
         "test symbol should match max length");
  Expect(symbol.Assign(max_length), "max-length symbol should be accepted");
  Expect(symbol.view() == max_length, "max-length symbol should round-trip");
}

}  // namespace

int main() {
  TestDefaultSymbolIsEmpty();
  TestAssignValidSymbol();
  TestConstructorFromStringView();
  TestAssignmentOperator();
  TestRejectEmptySymbol();
  TestRejectTooLongSymbol();
  TestMaxLengthSymbol();

  std::cout << "All fixed symbol tests passed.\n";
  return 0;
}
