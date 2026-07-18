#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>

#include "llgw/order_lifecycle.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

void TestAcceptedLifecycle() {
  llgw::OrderLifecycleTracker tracker;

  Expect(tracker.RegisterCreated(1), "new order should register");

  auto record = tracker.Find(1);
  Expect(record.has_value(), "registered order should be found");
  Expect(
      record->state == llgw::OrderLifecycleState::kCreated,
      "new order should start in created state");

  Expect(tracker.MarkQueued(1), "created order should queue");
  Expect(tracker.MarkSent(1), "queued order should send");
  Expect(
      tracker.MarkExchangeAccepted(1),
      "sent order should become exchange accepted");

  record = tracker.Find(1);
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kExchangeAccepted,
      "accepted order should have terminal state");
  Expect(tracker.orders_registered() == 1, "one order registered");
  Expect(tracker.orders_queued() == 1, "one order queued");
  Expect(tracker.orders_sent() == 1, "one order sent");
  Expect(
      tracker.orders_exchange_accepted() == 1,
      "one order exchange accepted");

  Expect(
      !tracker.MarkSent(1),
      "terminal order should reject another transition");
  Expect(
      tracker.last_error() ==
          llgw::LifecycleError::kInvalidTransition,
      "illegal terminal transition should be classified");
}

void TestRiskRejectedLifecyclePreservesReason() {
  llgw::OrderLifecycleTracker tracker;

  Expect(tracker.RegisterQueued(2), "order should register queued");
  Expect(
      tracker.MarkRiskRejected(
          2,
          llgw::RiskRejectReason::kNotionalLimit),
      "queued order should support risk rejection");

  const auto record = tracker.Find(2);
  Expect(record.has_value(), "risk-rejected order should exist");
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kRiskRejected,
      "order should have terminal risk state");
  Expect(
      record->risk_reject_reason ==
          llgw::RiskRejectReason::kNotionalLimit,
      "risk reject reason should be preserved");
  Expect(
      !tracker.MarkSent(2),
      "risk-rejected order must not transition to sent");
}

void TestExchangeRejectedLifecyclePreservesReason() {
  llgw::OrderLifecycleTracker tracker;

  Expect(tracker.RegisterQueued(3), "order should queue");
  Expect(tracker.MarkSent(3), "queued order should send");
  Expect(
      tracker.MarkExchangeRejected(
          3,
          llgw::OrderRejectReason::kDuplicateSequence),
      "sent order should support exchange rejection");

  const auto record = tracker.Find(3);
  Expect(record.has_value(), "exchange-rejected order should exist");
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kExchangeRejected,
      "order should have terminal exchange-rejected state");
  Expect(
      record->exchange_reject_reason ==
          llgw::OrderRejectReason::kDuplicateSequence,
      "exchange reject reason should be preserved");
}

void TestDuplicateAndUnknownOrdersAreRejected() {
  llgw::OrderLifecycleTracker tracker;

  Expect(tracker.RegisterCreated(9), "first registration should pass");
  Expect(
      !tracker.RegisterCreated(9),
      "duplicate registration should fail");
  Expect(
      tracker.last_error() ==
          llgw::LifecycleError::kDuplicateOrder,
      "duplicate should have explicit error");
  Expect(
      tracker.duplicate_order_errors() == 1,
      "duplicate counter should increment");

  Expect(!tracker.MarkQueued(99), "unknown transition should fail");
  Expect(
      tracker.last_error() ==
          llgw::LifecycleError::kUnknownOrder,
      "unknown transition should have explicit error");

  Expect(
      std::string_view(llgw::ToString(
          llgw::OrderLifecycleState::kExchangeAccepted)) ==
          "exchange_accepted",
      "state string should be stable");
  Expect(
      std::string_view(llgw::ToString(
          llgw::LifecycleError::kDuplicateOrder)) ==
          "duplicate_order",
      "error string should be stable");
}

}  // namespace

int main() {
  TestAcceptedLifecycle();
  TestRiskRejectedLifecyclePreservesReason();
  TestExchangeRejectedLifecyclePreservesReason();
  TestDuplicateAndUnknownOrdersAreRejected();

  std::cout << "All order lifecycle tests passed.\n";
  return 0;
}
