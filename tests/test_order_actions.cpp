#include <cstdlib>
#include <iostream>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_lifecycle.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::OrderRequest MakeOrder(std::uint64_t sequence_id) {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = "AAPL";
  request.side = llgw::Side::kBuy;
  request.price = 100.0;
  request.quantity = 10;
  return request;
}

void RegisterAcceptedLifecycle(
    llgw::OrderLifecycleTracker* lifecycle,
    std::uint64_t sequence_id) {
  Expect(lifecycle->RegisterQueued(sequence_id), "order should queue");
  Expect(lifecycle->MarkSent(sequence_id), "order should send");
  Expect(
      lifecycle->MarkExchangeAccepted(sequence_id),
      "order should become exchange accepted");
}

void TestAcceptedCancelRemovesActiveOrder() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderGateway gateway(&exchange, &lifecycle);

  Expect(
      exchange.SubmitOrder(MakeOrder(1)).accepted,
      "order should become active");
  RegisterAcceptedLifecycle(&lifecycle, 1);

  llgw::CancelRequest request{};
  request.sequence_id = 1;

  const llgw::CancelResponse response =
      gateway.SendCancel(request);

  Expect(response.accepted, "active order should cancel");
  Expect(!exchange.HasActiveOrder(1), "cancel should remove order");
  Expect(gateway.cancels_sent() == 1, "one cancel sent");
  Expect(gateway.cancels_accepted() == 1, "one cancel accepted");
  Expect(
      lifecycle.Find(1)->state ==
          llgw::OrderLifecycleState::kCancelled,
      "lifecycle should become cancelled");
}

void TestUnknownCancelRejected() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);

  llgw::CancelRequest request{};
  request.sequence_id = 99;

  const llgw::CancelResponse response =
      gateway.SendCancel(request);

  Expect(!response.accepted, "unknown cancel should reject");
  Expect(
      response.reject_reason ==
          llgw::CancelRejectReason::kUnknownOrder,
      "unknown cancel reason should be preserved");
  Expect(gateway.cancels_rejected() == 1, "cancel reject counted");
}

void TestAcceptedAmendUpdatesActiveOrder() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderGateway gateway(&exchange, &lifecycle);

  Expect(
      exchange.SubmitOrder(MakeOrder(2)).accepted,
      "order should become active");
  RegisterAcceptedLifecycle(&lifecycle, 2);

  llgw::AmendRequest request{};
  request.sequence_id = 2;
  request.new_price = 101.5;
  request.new_quantity = 25;

  const llgw::AmendResponse response =
      gateway.SendAmend(request);

  Expect(response.accepted, "valid amend should pass");

  const auto active = exchange.FindActiveOrder(2);
  Expect(active.has_value(), "amended order should remain active");
  Expect(active->price == 101.5, "amend should update price");
  Expect(active->quantity == 25, "amend should update quantity");
  Expect(
      lifecycle.Find(2)->state ==
          llgw::OrderLifecycleState::kExchangeAccepted,
      "accepted amend should restore active state");
  Expect(lifecycle.amends_accepted() == 1, "amend counted");
}

void TestRejectedAmendPreservesOriginalOrder() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderGateway gateway(&exchange, &lifecycle);

  Expect(
      exchange.SubmitOrder(MakeOrder(3)).accepted,
      "order should become active");
  RegisterAcceptedLifecycle(&lifecycle, 3);

  llgw::AmendRequest request{};
  request.sequence_id = 3;
  request.new_price = 0.0;
  request.new_quantity = 50;

  const llgw::AmendResponse response =
      gateway.SendAmend(request);

  Expect(!response.accepted, "invalid amend should reject");
  Expect(
      response.reject_reason ==
          llgw::AmendRejectReason::kInvalidPrice,
      "invalid-price reason should be preserved");

  const auto active = exchange.FindActiveOrder(3);
  Expect(active.has_value(), "rejected amend should stay active");
  Expect(active->price == 100.0, "original price should remain");
  Expect(active->quantity == 10, "original quantity should remain");

  const auto record = lifecycle.Find(3);
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kExchangeAccepted,
      "rejected amend should restore accepted state");
  Expect(
      record->amend_reject_reason ==
          llgw::AmendRejectReason::kInvalidPrice,
      "lifecycle should preserve amend rejection");
}

}  // namespace

int main() {
  TestAcceptedCancelRemovesActiveOrder();
  TestUnknownCancelRejected();
  TestAcceptedAmendUpdatesActiveOrder();
  TestRejectedAmendPreservesOriginalOrder();

  std::cout << "All order action tests passed.\n";
  return 0;
}
