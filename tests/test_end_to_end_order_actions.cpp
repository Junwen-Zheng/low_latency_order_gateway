#include <cstdlib>
#include <iostream>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_lifecycle.hpp"
#include "llgw/order_pipeline.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::OrderRequest MakeOrder(
    std::uint64_t sequence_id,
    double price,
    std::uint32_t quantity) {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = "AAPL";
  request.side = llgw::Side::kBuy;
  request.price = price;
  request.quantity = quantity;
  return request;
}

void TestSubmitAmendCancelFlow() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderGateway gateway(&exchange, &lifecycle);
  llgw::OrderPipeline<4> pipeline(&gateway, nullptr, &lifecycle);

  Expect(
      pipeline.Enqueue(MakeOrder(10, 100.0, 10)),
      "order should enqueue");
  Expect(pipeline.Drain() == 1, "one order should drain");
  Expect(exchange.HasActiveOrder(10), "order should be active");

  llgw::AmendRequest amend{};
  amend.sequence_id = 10;
  amend.new_price = 101.25;
  amend.new_quantity = 20;

  const llgw::AmendResponse amend_response =
      gateway.SendAmend(amend);

  Expect(amend_response.accepted, "amend should pass");

  const auto amended = exchange.FindActiveOrder(10);
  Expect(amended.has_value(), "amended order should exist");
  Expect(amended->price == 101.25, "price should update");
  Expect(amended->quantity == 20, "quantity should update");

  llgw::CancelRequest cancel{};
  cancel.sequence_id = 10;

  const llgw::CancelResponse cancel_response =
      gateway.SendCancel(cancel);

  Expect(cancel_response.accepted, "cancel should pass");
  Expect(!exchange.HasActiveOrder(10), "order should be removed");

  const auto record = lifecycle.Find(10);
  Expect(record.has_value(), "order should remain tracked");
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kCancelled,
      "final state should be cancelled");
  Expect(lifecycle.amends_accepted() == 1, "one amend accepted");
  Expect(lifecycle.cancels_accepted() == 1, "one cancel accepted");
  Expect(
      gateway.lifecycle_transition_errors() == 0,
      "valid flow should have no lifecycle errors");
}

void TestActionAfterCancellationRejectedBeforeExchange() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderGateway gateway(&exchange, &lifecycle);
  llgw::OrderPipeline<4> pipeline(&gateway, nullptr, &lifecycle);

  Expect(
      pipeline.Enqueue(MakeOrder(20, 100.0, 10)),
      "order should enqueue");
  pipeline.Drain();

  llgw::CancelRequest cancel{};
  cancel.sequence_id = 20;
  Expect(gateway.SendCancel(cancel).accepted, "cancel should pass");

  llgw::AmendRequest amend{};
  amend.sequence_id = 20;
  amend.new_price = 105.0;
  amend.new_quantity = 10;

  const llgw::AmendResponse response =
      gateway.SendAmend(amend);

  Expect(!response.accepted, "post-cancel amend should reject");
  Expect(
      response.reject_reason ==
          llgw::AmendRejectReason::kInvalidState,
      "illegal lifecycle state should be explicit");
  Expect(
      gateway.lifecycle_transition_errors() == 1,
      "illegal action should be counted");
  Expect(
      lifecycle.Find(20)->state ==
          llgw::OrderLifecycleState::kCancelled,
      "cancelled state must remain unchanged");
  Expect(gateway.amends_sent() == 1, "attempt counted");
  Expect(gateway.amends_rejected() == 1, "rejection counted");
}

}  // namespace

int main() {
  TestSubmitAmendCancelFlow();
  TestActionAfterCancellationRejectedBeforeExchange();

  std::cout << "All end-to-end order action tests passed.\n";
  return 0;
}
