#include <cstdlib>
#include <iostream>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::OrderRequest MakeOrder(std::uint64_t sequence_id, std::uint32_t quantity = 100) {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = "AAPL";
  request.side = llgw::Side::kBuy;
  request.price = 192.10;
  request.quantity = quantity;
  return request;
}

void TestGatewayTracksAcceptedOrder() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);

  const llgw::OrderResponse response = gateway.SendOrder(MakeOrder(1));

  Expect(response.accepted, "valid order should be accepted");
  Expect(gateway.orders_sent() == 1, "orders sent should be 1");
  Expect(gateway.orders_accepted() == 1, "orders accepted should be 1");
  Expect(gateway.orders_rejected() == 0, "orders rejected should be 0");
}

void TestGatewayTracksRejectedOrder() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);

  const llgw::OrderResponse response = gateway.SendOrder(MakeOrder(1, 0));

  Expect(!response.accepted, "invalid order should be rejected");
  Expect(response.reject_reason == llgw::OrderRejectReason::kInvalidQuantity,
         "reject reason should be invalid quantity");
  Expect(gateway.orders_sent() == 1, "orders sent should be 1");
  Expect(gateway.orders_accepted() == 0, "orders accepted should be 0");
  Expect(gateway.orders_rejected() == 1, "orders rejected should be 1");
}

void TestGatewayTracksDuplicateSequence() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);

  const llgw::OrderResponse first = gateway.SendOrder(MakeOrder(7));
  const llgw::OrderResponse second = gateway.SendOrder(MakeOrder(7));

  Expect(first.accepted, "first order should be accepted");
  Expect(!second.accepted, "duplicate order should be rejected");
  Expect(second.reject_reason == llgw::OrderRejectReason::kDuplicateSequence,
         "reject reason should be duplicate sequence");
  Expect(gateway.orders_sent() == 2, "orders sent should be 2");
  Expect(gateway.orders_accepted() == 1, "orders accepted should be 1");
  Expect(gateway.orders_rejected() == 1, "orders rejected should be 1");
}

}  // namespace

int main() {
  TestGatewayTracksAcceptedOrder();
  TestGatewayTracksRejectedOrder();
  TestGatewayTracksDuplicateSequence();

  std::cout << "All order gateway tests passed.\n";
  return 0;
}
