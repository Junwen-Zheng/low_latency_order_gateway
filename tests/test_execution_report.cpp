#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "llgw/execution_report.hpp"
#include "llgw/order_lifecycle.hpp"

namespace {

void Expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "Test failed: " << message << "\n";
    std::exit(1);
  }
}

void TestDeterministicAcceptedActionOrdering() {
  llgw::ExecutionReportJournal reports;
  llgw::OrderLifecycleTracker lifecycle(&reports);

  Expect(lifecycle.RegisterQueued(1), "order should queue");
  Expect(lifecycle.MarkSent(1), "order should send");
  Expect(
      lifecycle.MarkExchangeAccepted(1),
      "order should become accepted");
  Expect(
      lifecycle.MarkAmendPending(1),
      "order should enter amend pending");
  Expect(
      lifecycle.MarkAmendAccepted(1),
      "amend should return order to accepted");
  Expect(
      lifecycle.MarkCancelPending(1),
      "order should enter cancel pending");
  Expect(lifecycle.MarkCancelled(1), "order should cancel");

  const std::vector<llgw::ExecutionReport> order_reports =
      reports.ReportsFor(1);

  Expect(order_reports.size() == 8, "eight reports expected");

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

  for (std::size_t i = 0; i < order_reports.size(); ++i) {
    Expect(
        order_reports[i].event_index ==
            static_cast<std::uint64_t>(i + 1),
        "event indexes should be monotonic and one based");
    Expect(
        order_reports[i].type == expected[i],
        "report ordering should be deterministic");
  }

  Expect(reports.order_reports() == 4, "four order reports");
  Expect(reports.action_reports() == 4, "four action reports");
  Expect(reports.rejection_reports() == 0, "no rejections");
}

void TestStructuredRejectionReports() {
  llgw::ExecutionReportJournal reports;
  llgw::OrderLifecycleTracker lifecycle(&reports);

  Expect(lifecycle.RegisterQueued(2), "risk order should queue");
  Expect(
      lifecycle.MarkRiskRejected(
          2,
          llgw::RiskRejectReason::kNotionalLimit),
      "risk rejection should record");

  Expect(lifecycle.RegisterQueued(3), "exchange order should queue");
  Expect(lifecycle.MarkSent(3), "exchange order should send");
  Expect(
      lifecycle.MarkExchangeRejected(
          3,
          llgw::OrderRejectReason::kDuplicateSequence),
      "exchange rejection should record");

  Expect(lifecycle.RegisterQueued(4), "amend order should queue");
  Expect(lifecycle.MarkSent(4), "amend order should send");
  Expect(
      lifecycle.MarkExchangeAccepted(4),
      "amend order should become accepted");
  Expect(
      lifecycle.MarkAmendPending(4),
      "amend should become pending");
  Expect(
      lifecycle.MarkAmendRejected(
          4,
          llgw::AmendRejectReason::kInvalidPrice),
      "amend rejection should record");

  const auto risk = reports.ReportsFor(2);
  const auto exchange = reports.ReportsFor(3);
  const auto amend = reports.ReportsFor(4);

  Expect(
      risk.back().reject_source ==
          llgw::ExecutionRejectSource::kRisk,
      "risk source should be structured");
  Expect(
      risk.back().reject_reason == "notional_limit",
      "risk reason should be stable");
  Expect(
      exchange.back().reject_source ==
          llgw::ExecutionRejectSource::kExchange,
      "exchange source should be structured");
  Expect(
      exchange.back().reject_reason == "duplicate_sequence",
      "exchange reason should be stable");
  Expect(
      amend.back().type ==
          llgw::ExecutionReportType::kAmendRejected,
      "amend rejection type should be explicit");
  Expect(
      amend.back().reject_reason == "invalid_price",
      "amend reason should be stable");
  Expect(reports.rejection_reports() == 3, "three rejections");
}

void TestStableTextFormatting() {
  llgw::ExecutionReportJournal reports;
  reports.RecordExchangeRejected(
      42,
      llgw::OrderRejectReason::kDuplicateSequence);

  const std::string formatted =
      llgw::FormatExecutionReport(reports.reports().front());

  Expect(
      formatted ==
          "event=1 sequence_id=42 type=exchange_rejected "
          "reject_source=exchange "
          "reject_reason=duplicate_sequence",
      "report formatting should be stable");
}

}  // namespace

int main() {
  TestDeterministicAcceptedActionOrdering();
  TestStructuredRejectionReports();
  TestStableTextFormatting();

  std::cout << "All execution report tests passed.\n";
  return 0;
}
