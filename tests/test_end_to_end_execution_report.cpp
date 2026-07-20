#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

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
    std::string_view symbol,
    double price,
    std::uint32_t quantity) {
  llgw::OrderRequest request{};
  request.sequence_id = sequence_id;
  request.symbol = symbol;
  request.side = llgw::Side::kBuy;
  request.price = price;
  request.quantity = quantity;
  return request;
}

void TestSubmitAmendCancelAndRiskReportFlow() {
  llgw::RiskLimits limits;
  limits.max_order_quantity = 100;
  limits.max_order_notional = 100'000.0;
  limits.allowed_symbols[0] = "AAPL";
  limits.allowed_symbol_count = 1;

  llgw::ExchangeSimulator exchange;
  llgw::ExecutionReportJournal reports;
  llgw::OrderLifecycleTracker lifecycle(&reports);
  llgw::OrderGateway gateway(&exchange, &lifecycle);
  llgw::PreTradeRiskManager risk(limits);
  llgw::OrderPipeline<8> pipeline(&gateway, &risk, &lifecycle);

  Expect(
      pipeline.Enqueue(MakeOrder(10, "AAPL", 100.0, 10)),
      "accepted order should enqueue");
  Expect(
      pipeline.Enqueue(MakeOrder(11, "NVDA", 100.0, 10)),
      "risk-rejected order should enqueue");
  Expect(pipeline.Drain() == 2, "two orders should drain");

  llgw::AmendRequest amend{};
  amend.sequence_id = 10;
  amend.new_price = 101.25;
  amend.new_quantity = 20;
  Expect(gateway.SendAmend(amend).accepted, "amend should pass");

  llgw::CancelRequest cancel{};
  cancel.sequence_id = 10;
  Expect(gateway.SendCancel(cancel).accepted, "cancel should pass");

  const std::vector<llgw::ExecutionReport> accepted =
      reports.ReportsFor(10);
  const std::vector<llgw::ExecutionReport> rejected =
      reports.ReportsFor(11);

  const llgw::ExecutionReportType expected[] = {
      llgw::ExecutionReportType::kCreated,
      llgw::ExecutionReportType::kQueued,
      llgw::ExecutionReportType::kSent,
      llgw::ExecutionReportType::kExchangeAccepted,
      llgw::ExecutionReportType::kAmendPending,
      llgw::ExecutionReportType::kAmendAccepted,
      llgw::ExecutionReportType::kCancelPending,
      llgw::ExecutionReportType::kCancelled,
  };

  Expect(accepted.size() == 8, "accepted flow should emit eight");
  for (std::size_t i = 0; i < accepted.size(); ++i) {
    Expect(
        accepted[i].type == expected[i],
        "accepted report ordering should match");
  }

  Expect(rejected.size() == 3, "risk flow should emit three");
  Expect(
      rejected.back().type ==
          llgw::ExecutionReportType::kRiskRejected,
      "risk path should end in rejection");
  Expect(
      rejected.back().reject_source ==
          llgw::ExecutionRejectSource::kRisk,
      "risk source should be structured");
  Expect(
      rejected.back().reject_reason == "symbol_not_allowed",
      "risk reason should be exact");
  Expect(gateway.orders_sent() == 1, "risk order stops early");
  Expect(reports.reports_recorded() == 11, "eleven reports");
  Expect(reports.rejection_reports() == 1, "one rejection");

  for (std::size_t i = 0; i < reports.reports().size(); ++i) {
    Expect(
        reports.reports()[i].event_index ==
            static_cast<std::uint64_t>(i + 1),
        "global ordering should be deterministic");
  }
}

}  // namespace

int main() {
  TestSubmitAmendCancelAndRiskReportFlow();

  std::cout
      << "All end-to-end execution report tests passed.\n";
  return 0;
}
