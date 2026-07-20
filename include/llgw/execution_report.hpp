#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "llgw/messages.hpp"
#include "llgw/pre_trade_risk.hpp"

namespace llgw {

enum class ExecutionReportType {
  kCreated,
  kQueued,
  kRiskRejected,
  kSent,
  kExchangeAccepted,
  kExchangeRejected,
  kCancelPending,
  kCancelled,
  kCancelRejected,
  kAmendPending,
  kAmendAccepted,
  kAmendRejected,
};

enum class ExecutionRejectSource {
  kNone,
  kRisk,
  kExchange,
  kCancel,
  kAmend,
};

struct ExecutionReport {
  std::uint64_t event_index = 0;
  std::uint64_t sequence_id = 0;
  ExecutionReportType type = ExecutionReportType::kCreated;
  ExecutionRejectSource reject_source = ExecutionRejectSource::kNone;
  std::string reject_reason = "none";
};

const char* ToString(ExecutionReportType type);
const char* ToString(ExecutionRejectSource source);
std::string FormatExecutionReport(const ExecutionReport& report);

class ExecutionReportJournal {
 public:
  void RecordState(
      std::uint64_t sequence_id,
      ExecutionReportType type);

  void RecordRiskRejected(
      std::uint64_t sequence_id,
      RiskRejectReason reason);
  void RecordExchangeRejected(
      std::uint64_t sequence_id,
      OrderRejectReason reason);
  void RecordCancelRejected(
      std::uint64_t sequence_id,
      CancelRejectReason reason);
  void RecordAmendRejected(
      std::uint64_t sequence_id,
      AmendRejectReason reason);

  const std::vector<ExecutionReport>& reports() const {
    return reports_;
  }

  std::vector<ExecutionReport> ReportsFor(
      std::uint64_t sequence_id) const;

  std::size_t size() const { return reports_.size(); }
  std::uint64_t reports_recorded() const {
    return reports_recorded_;
  }
  std::uint64_t rejection_reports() const {
    return rejection_reports_;
  }
  std::uint64_t order_reports() const {
    return order_reports_;
  }
  std::uint64_t action_reports() const {
    return action_reports_;
  }

 private:
  void Record(
      std::uint64_t sequence_id,
      ExecutionReportType type,
      ExecutionRejectSource reject_source,
      const char* reject_reason);

  std::vector<ExecutionReport> reports_;
  std::uint64_t next_event_index_ = 1;
  std::uint64_t reports_recorded_ = 0;
  std::uint64_t rejection_reports_ = 0;
  std::uint64_t order_reports_ = 0;
  std::uint64_t action_reports_ = 0;
};

}  // namespace llgw
