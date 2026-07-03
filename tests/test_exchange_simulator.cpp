#include <cstdlib>
#include <iostream>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::OrderRequest ValidOrder(std::uint64_t sequence_id) {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = "AAPL";
  request.side = llgw::Side::kBuy;
  request.price = 192.10;
  request.quantity = 100;
  return request;
}

void TestValidOrderAccepted() {
  llgw::ExchangeSimulator exchange;

  const llgw::OrderResponse response = exchange.SubmitOrder(ValidOrder(1));

  Expect(response.sequence_id == 1, "response sequence id should match request");
  Expect(response.accepted, "valid order should be accepted");
  Expect(response.reject_reason == llgw::OrderRejectReason::kNone,
         "accepted order should have no reject reason");
}

void TestEmptySymbolRejected() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderRequest request = ValidOrder(1);
  request.symbol = "";

  const llgw::OrderResponse response = exchange.SubmitOrder(request);

  Expect(!response.accepted, "empty symbol should be rejected");
  Expect(response.reject_reason == llgw::OrderRejectReason::kInvalidSymbol,
         "reject reason should be invalid symbol");
}

void TestInvalidPriceRejected() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderRequest request = ValidOrder(1);
  request.price = 0.0;

  const llgw::OrderResponse response = exchange.SubmitOrder(request);

  Expect(!response.accepted, "zero price should be rejected");
  Expect(response.reject_reason == llgw::OrderRejectReason::kInvalidPrice,
         "reject reason should be invalid price");
}

void TestInvalidQuantityRejected() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderRequest request = ValidOrder(1);
  request.quantity = 0;

  const llgw::OrderResponse response = exchange.SubmitOrder(request);

  Expect(!response.accepted, "zero quantity should be rejected");
  Expect(response.reject_reason == llgw::OrderRejectReason::kInvalidQuantity,
         "reject reason should be invalid quantity");
}

void TestDuplicateSequenceRejected() {
  llgw::ExchangeSimulator exchange;

  const llgw::OrderResponse first = exchange.SubmitOrder(ValidOrder(42));
  const llgw::OrderResponse second = exchange.SubmitOrder(ValidOrder(42));

  Expect(first.accepted, "first order should be accepted");
  Expect(!second.accepted, "duplicate sequence should be rejected");
  Expect(second.reject_reason == llgw::OrderRejectReason::kDuplicateSequence,
         "reject reason should be duplicate sequence");
}

}  // namespace

int main() {
  TestValidOrderAccepted();
  TestEmptySymbolRejected();
  TestInvalidPriceRejected();
  TestInvalidQuantityRejected();
  TestDuplicateSequenceRejected();

  std::cout << "All exchange simulator tests passed.\n";
  return 0;
}
