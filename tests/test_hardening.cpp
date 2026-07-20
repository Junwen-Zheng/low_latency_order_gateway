#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string_view>

#include "llgw/exchange_simulator.hpp"
#include "llgw/execution_report.hpp"
#include "llgw/messages.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_lifecycle.hpp"
#include "llgw/order_pipeline.hpp"
#include "llgw/pre_trade_risk.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

llgw::OrderRequest MakeOrder(
    std::uint64_t sequence_id,
    std::string_view symbol = "AAPL",
    double price = 100.0,
    std::uint32_t quantity = 10) {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = symbol;
  request.side = llgw::Side::kBuy;
  request.price = price;
  request.quantity = quantity;
  return request;
}

void RegisterAcceptedLifecycle(
    llgw::OrderLifecycleTracker* lifecycle,
    std::uint64_t sequence_id) {
  Expect(
      lifecycle->RegisterQueued(sequence_id),
      "hardening order should register queued");
  Expect(
      lifecycle->MarkSent(sequence_id),
      "hardening order should transition to sent");
  Expect(
      lifecycle->MarkExchangeAccepted(sequence_id),
      "hardening order should become exchange accepted");
}

void TestNonFiniteSubmissionPricesAreRejected() {
  llgw::ExchangeSimulator exchange;

  const llgw::OrderResponse infinity_response =
      exchange.SubmitOrder(MakeOrder(
          1,
          "AAPL",
          std::numeric_limits<double>::infinity(),
          10));

  const llgw::OrderResponse nan_response =
      exchange.SubmitOrder(MakeOrder(
          2,
          "AAPL",
          std::numeric_limits<double>::quiet_NaN(),
          10));

  Expect(
      !infinity_response.accepted,
      "infinite order price should be rejected");
  Expect(
      infinity_response.reject_reason ==
          llgw::OrderRejectReason::kInvalidPrice,
      "infinite order price should use invalid-price reason");
  Expect(
      !nan_response.accepted,
      "NaN order price should be rejected");
  Expect(
      nan_response.reject_reason ==
          llgw::OrderRejectReason::kInvalidPrice,
      "NaN order price should use invalid-price reason");
  Expect(
      exchange.active_order_count() == 0,
      "non-finite orders must not enter the active store");
}

void TestRejectedAmendPreservesActiveOrder() {
  llgw::ExchangeSimulator exchange;

  Expect(
      exchange.SubmitOrder(MakeOrder(10)).accepted,
      "baseline order should be accepted");

  llgw::AmendRequest amend{};
  amend.sequence_id = 10;
  amend.new_price = std::numeric_limits<double>::infinity();
  amend.new_quantity = 20;

  const llgw::AmendResponse response =
      exchange.AmendOrder(amend);

  Expect(
      !response.accepted,
      "infinite amend price should be rejected");
  Expect(
      response.reject_reason ==
          llgw::AmendRejectReason::kInvalidPrice,
      "infinite amend price should use invalid-price reason");

  const auto active = exchange.FindActiveOrder(10);
  Expect(
      active.has_value(),
      "rejected amend should leave the order active");
  Expect(
      active->price == 100.0,
      "rejected amend should preserve original price");
  Expect(
      active->quantity == 10,
      "rejected amend should preserve original quantity");
}

void TestCancelledSequenceCannotBeReused() {
  llgw::ExchangeSimulator exchange;

  Expect(
      exchange.SubmitOrder(MakeOrder(20)).accepted,
      "first sequence use should be accepted");

  llgw::CancelRequest cancel{};
  cancel.sequence_id = 20;

  Expect(
      exchange.CancelOrder(cancel).accepted,
      "active order should cancel");
  Expect(
      !exchange.HasActiveOrder(20),
      "cancel should remove active order");

  const llgw::OrderResponse duplicate =
      exchange.SubmitOrder(MakeOrder(20));

  Expect(
      !duplicate.accepted,
      "cancelled sequence ID must not be reused");
  Expect(
      duplicate.reject_reason ==
          llgw::OrderRejectReason::kDuplicateSequence,
      "reused sequence should remain a duplicate");
  Expect(
      exchange.active_order_count() == 0,
      "reused sequence must not recreate an active order");
}

void TestAllowlistCountIsClampedToStorage() {
  llgw::RiskLimits limits;
  limits.max_order_quantity = 100;
  limits.max_order_notional = 100'000.0;
  limits.allowed_symbols[0] = "AAPL";
  limits.allowed_symbols[1] = "MSFT";
  limits.allowed_symbols[2] = "NVDA";
  limits.allowed_symbols[3] = "GOOG";
  limits.allowed_symbols[4] = "TSLA";
  limits.allowed_symbols[5] = "AMZN";
  limits.allowed_symbols[6] = "META";
  limits.allowed_symbols[7] = "SPY";
  limits.allowed_symbol_count = 999;

  llgw::PreTradeRiskManager risk(limits);

  const llgw::RiskDecision allowed =
      risk.Check(MakeOrder(30, "SPY"));
  const llgw::RiskDecision rejected =
      risk.Check(MakeOrder(31, "IBM"));

  Expect(
      allowed.accepted,
      "last stored allowlist symbol should remain valid");
  Expect(
      !rejected.accepted,
      "symbol outside clamped allowlist should reject");
  Expect(
      rejected.reject_reason ==
          llgw::RiskRejectReason::kSymbolNotAllowed,
      "clamped allowlist should preserve rejection reason");
}

void TestNullExchangeActionsRollbackLifecycle() {
  llgw::ExecutionReportJournal reports;
  llgw::OrderLifecycleTracker lifecycle(&reports);
  llgw::OrderGateway gateway(nullptr, &lifecycle);

  RegisterAcceptedLifecycle(&lifecycle, 40);

  llgw::AmendRequest amend{};
  amend.sequence_id = 40;
  amend.new_price = 101.0;
  amend.new_quantity = 20;

  const llgw::AmendResponse amend_response =
      gateway.SendAmend(amend);

  Expect(
      !amend_response.accepted,
      "amend without exchange should reject");
  Expect(
      amend_response.reject_reason ==
          llgw::AmendRejectReason::kUnknownOrder,
      "null-exchange amend should preserve unknown-order reason");

  auto record = lifecycle.Find(40);
  Expect(
      record.has_value(),
      "lifecycle record should remain available");
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kExchangeAccepted,
      "rejected amend should roll back to accepted state");
  Expect(
      record->amend_reject_reason ==
          llgw::AmendRejectReason::kUnknownOrder,
      "lifecycle should preserve null-exchange amend reason");

  llgw::CancelRequest cancel{};
  cancel.sequence_id = 40;

  const llgw::CancelResponse cancel_response =
      gateway.SendCancel(cancel);

  Expect(
      !cancel_response.accepted,
      "cancel without exchange should reject");
  Expect(
      cancel_response.reject_reason ==
          llgw::CancelRejectReason::kUnknownOrder,
      "null-exchange cancel should preserve unknown-order reason");

  record = lifecycle.Find(40);
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kExchangeAccepted,
      "rejected cancel should roll back to accepted state");
  Expect(
      record->cancel_reject_reason ==
          llgw::CancelRejectReason::kUnknownOrder,
      "lifecycle should preserve null-exchange cancel reason");
  Expect(
      gateway.lifecycle_transition_errors() == 0,
      "valid rollback transitions should not be errors");
  Expect(
      reports.rejection_reports() == 2,
      "both null-exchange actions should emit rejection reports");
}

void TestNullGatewayPipelineEndsInExchangeReject() {
  llgw::OrderLifecycleTracker lifecycle;
  llgw::OrderPipeline<2> pipeline(
      nullptr,
      nullptr,
      &lifecycle);

  Expect(
      pipeline.Enqueue(MakeOrder(50)),
      "null-gateway pipeline should still queue deterministically");
  Expect(
      pipeline.Drain() == 1,
      "null-gateway pipeline should drain one order");

  const auto record = lifecycle.Find(50);
  Expect(
      record.has_value(),
      "null-gateway order should remain tracked");
  Expect(
      record->state ==
          llgw::OrderLifecycleState::kExchangeRejected,
      "null gateway should terminate in exchange-rejected state");
  Expect(
      pipeline.orders_rejected_by_gateway() == 1,
      "null gateway should increment gateway rejection count");
  Expect(
      pipeline.lifecycle_transition_errors() == 0,
      "null-gateway path should preserve legal transitions");
}

void TestUnknownReportQueryIsEmpty() {
  llgw::ExecutionReportJournal reports;

  Expect(
      reports.ReportsFor(999).empty(),
      "unknown sequence report query should be empty");
  Expect(
      reports.size() == 0,
      "unknown query must not mutate journal state");
}

}  // namespace

int main() {
  TestNonFiniteSubmissionPricesAreRejected();
  TestRejectedAmendPreservesActiveOrder();
  TestCancelledSequenceCannotBeReused();
  TestAllowlistCountIsClampedToStorage();
  TestNullExchangeActionsRollbackLifecycle();
  TestNullGatewayPipelineEndsInExchangeReject();
  TestUnknownReportQueryIsEmpty();

  std::cout << "All hardening tests passed.\n";
  return 0;
}
