#include <cstdlib>
#include <iostream>

#include "llgw/exchange_simulator.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_pipeline.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::OrderRequest MakeOrder(std::uint64_t sequence_id, std::string_view symbol = "AAPL") {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = symbol;
  request.side = llgw::Side::kBuy;
  request.price = 192.10;
  request.quantity = 100;
  return request;
}

void TestEmptyDrainDoesNothing() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderPipeline<4> pipeline(&gateway);

  const std::size_t drained = pipeline.Drain();

  Expect(drained == 0, "empty drain should drain zero orders");
  Expect(pipeline.orders_enqueued() == 0, "no orders should be enqueued");
  Expect(pipeline.orders_drained() == 0, "no orders should be drained");
  Expect(pipeline.orders_accepted() == 0, "no orders should be accepted");
  Expect(pipeline.orders_rejected() == 0, "no orders should be rejected");
}

void TestEnqueueAndDrainAcceptedOrders() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderPipeline<4> pipeline(&gateway);

  Expect(pipeline.Enqueue(MakeOrder(1)), "enqueue order 1 should succeed");
  Expect(pipeline.Enqueue(MakeOrder(2)), "enqueue order 2 should succeed");
  Expect(pipeline.Size() == 2, "pipeline size should be 2 before drain");

  const std::size_t drained = pipeline.Drain();

  Expect(drained == 2, "drain should process 2 orders");
  Expect(pipeline.Empty(), "pipeline should be empty after drain");

  Expect(pipeline.orders_enqueued() == 2, "pipeline should record 2 enqueued orders");
  Expect(pipeline.orders_dropped_on_enqueue() == 0, "pipeline should drop zero orders");
  Expect(pipeline.orders_drained() == 2, "pipeline should record 2 drained orders");
  Expect(pipeline.orders_accepted() == 2, "pipeline should record 2 accepted orders");
  Expect(pipeline.orders_rejected() == 0, "pipeline should record 0 rejected orders");

  Expect(gateway.orders_sent() == 2, "gateway should send 2 orders");
  Expect(gateway.orders_accepted() == 2, "gateway should accept 2 orders");
  Expect(gateway.orders_rejected() == 0, "gateway should reject 0 orders");
}

void TestRejectWhenPipelineFull() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderPipeline<2> pipeline(&gateway);

  Expect(pipeline.Enqueue(MakeOrder(1)), "enqueue order 1 should succeed");
  Expect(pipeline.Enqueue(MakeOrder(2)), "enqueue order 2 should succeed");
  Expect(!pipeline.Enqueue(MakeOrder(3)), "enqueue order 3 should fail when full");

  Expect(pipeline.Full(), "pipeline should be full");
  Expect(pipeline.Size() == 2, "pipeline should keep size 2");
  Expect(pipeline.orders_enqueued() == 2, "only 2 orders should be enqueued");
  Expect(pipeline.orders_dropped_on_enqueue() == 1, "1 order should be dropped on enqueue");

  const std::size_t drained = pipeline.Drain();

  Expect(drained == 2, "drain should process only 2 orders");
  Expect(pipeline.orders_accepted() == 2, "2 accepted orders should be recorded");
  Expect(gateway.orders_sent() == 2, "gateway should only see 2 orders");
}

void TestDrainTracksExchangeRejects() {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway(&exchange);
  llgw::OrderPipeline<4> pipeline(&gateway);

  Expect(pipeline.Enqueue(MakeOrder(7)), "first duplicate-sequence order should enqueue");
  Expect(pipeline.Enqueue(MakeOrder(7)), "second duplicate-sequence order should enqueue");

  const std::size_t drained = pipeline.Drain();

  Expect(drained == 2, "drain should process 2 orders");
  Expect(pipeline.orders_accepted() == 1, "first order should be accepted");
  Expect(pipeline.orders_rejected() == 1, "duplicate order should be rejected");

  Expect(gateway.orders_sent() == 2, "gateway should send 2 orders");
  Expect(gateway.orders_accepted() == 1, "gateway should accept 1 order");
  Expect(gateway.orders_rejected() == 1, "gateway should reject 1 order");
}

}  // namespace

int main() {
  TestEmptyDrainDoesNothing();
  TestEnqueueAndDrainAcceptedOrders();
  TestRejectWhenPipelineFull();
  TestDrainTracksExchangeRejects();

  std::cout << "All order pipeline tests passed.\n";
  return 0;
}
