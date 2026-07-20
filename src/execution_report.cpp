#include "llgw/execution_report.hpp"

#include <sstream>

namespace llgw {
namespace {

const char* OrderReasonString(OrderRejectReason reason) {
  switch (reason) {
    case OrderRejectReason::kNone:
      return "none";
    case OrderRejectReason::kInvalidSymbol:
      return "invalid_symbol";
    case OrderRejectReason::kInvalidPrice:
      return "invalid_price";
    case OrderRejectReason::kInvalidQuantity:
      return "invalid_quantity";
    case OrderRejectReason::kDuplicateSequence:
      return "duplicate_sequence";
  }

  return "unknown";
}

const char* CancelReasonString(CancelRejectReason reason) {
  switch (reason) {
    case CancelRejectReason::kNone:
      return "none";
    case CancelRejectReason::kUnknownOrder:
      return "unknown_order";
    case CancelRejectReason::kInvalidState:
      return "invalid_state";
  }

  return "unknown";
}

const char* AmendReasonString(AmendRejectReason reason) {
  switch (reason) {
    case AmendRejectReason::kNone:
      return "none";
    case AmendRejectReason::kUnknownOrder:
      return "unknown_order";
    case AmendRejectReason::kInvalidPrice:
      return "invalid_price";
    case AmendRejectReason::kInvalidQuantity:
      return "invalid_quantity";
    case AmendRejectReason::kInvalidState:
      return "invalid_state";
  }

  return "unknown";
}

bool IsActionReport(ExecutionReportType type) {
  switch (type) {
    case ExecutionReportType::kCancelPending:
    case ExecutionReportType::kCancelled:
    case ExecutionReportType::kCancelRejected:
    case ExecutionReportType::kAmendPending:
    case ExecutionReportType::kAmendAccepted:
    case ExecutionReportType::kAmendRejected:
      return true;
    case ExecutionReportType::kCreated:
    case ExecutionReportType::kQueued:
    case ExecutionReportType::kRiskRejected:
    case ExecutionReportType::kSent:
    case ExecutionReportType::kExchangeAccepted:
    case ExecutionReportType::kExchangeRejected:
      return false;
  }

  return false;
}

}  // namespace

const char* ToString(ExecutionReportType type) {
  switch (type) {
    case ExecutionReportType::kCreated:
      return "created";
    case ExecutionReportType::kQueued:
      return "queued";
    case ExecutionReportType::kRiskRejected:
      return "risk_rejected";
    case ExecutionReportType::kSent:
      return "sent";
    case ExecutionReportType::kExchangeAccepted:
      return "exchange_accepted";
    case ExecutionReportType::kExchangeRejected:
      return "exchange_rejected";
    case ExecutionReportType::kCancelPending:
      return "cancel_pending";
    case ExecutionReportType::kCancelled:
      return "cancelled";
    case ExecutionReportType::kCancelRejected:
      return "cancel_rejected";
    case ExecutionReportType::kAmendPending:
      return "amend_pending";
    case ExecutionReportType::kAmendAccepted:
      return "amend_accepted";
    case ExecutionReportType::kAmendRejected:
      return "amend_rejected";
  }

  return "unknown";
}

const char* ToString(ExecutionRejectSource source) {
  switch (source) {
    case ExecutionRejectSource::kNone:
      return "none";
    case ExecutionRejectSource::kRisk:
      return "risk";
    case ExecutionRejectSource::kExchange:
      return "exchange";
    case ExecutionRejectSource::kCancel:
      return "cancel";
    case ExecutionRejectSource::kAmend:
      return "amend";
  }

  return "unknown";
}

std::string FormatExecutionReport(
    const ExecutionReport& report) {
  std::ostringstream output;
  output
      << "event=" << report.event_index
      << " sequence_id=" << report.sequence_id
      << " type=" << ToString(report.type)
      << " reject_source=" << ToString(report.reject_source)
      << " reject_reason=" << report.reject_reason;
  return output.str();
}

void ExecutionReportJournal::RecordState(
    std::uint64_t sequence_id,
    ExecutionReportType type) {
  Record(
      sequence_id,
      type,
      ExecutionRejectSource::kNone,
      "none");
}

void ExecutionReportJournal::RecordRiskRejected(
    std::uint64_t sequence_id,
    RiskRejectReason reason) {
  Record(
      sequence_id,
      ExecutionReportType::kRiskRejected,
      ExecutionRejectSource::kRisk,
      ToString(reason));
}

void ExecutionReportJournal::RecordExchangeRejected(
    std::uint64_t sequence_id,
    OrderRejectReason reason) {
  Record(
      sequence_id,
      ExecutionReportType::kExchangeRejected,
      ExecutionRejectSource::kExchange,
      OrderReasonString(reason));
}

void ExecutionReportJournal::RecordCancelRejected(
    std::uint64_t sequence_id,
    CancelRejectReason reason) {
  Record(
      sequence_id,
      ExecutionReportType::kCancelRejected,
      ExecutionRejectSource::kCancel,
      CancelReasonString(reason));
}

void ExecutionReportJournal::RecordAmendRejected(
    std::uint64_t sequence_id,
    AmendRejectReason reason) {
  Record(
      sequence_id,
      ExecutionReportType::kAmendRejected,
      ExecutionRejectSource::kAmend,
      AmendReasonString(reason));
}

std::vector<ExecutionReport>
ExecutionReportJournal::ReportsFor(
    std::uint64_t sequence_id) const {
  std::vector<ExecutionReport> matches;

  for (const ExecutionReport& report : reports_) {
    if (report.sequence_id == sequence_id) {
      matches.push_back(report);
    }
  }

  return matches;
}

void ExecutionReportJournal::Record(
    std::uint64_t sequence_id,
    ExecutionReportType type,
    ExecutionRejectSource reject_source,
    const char* reject_reason) {
  ExecutionReport report;
  report.event_index = next_event_index_++;
  report.sequence_id = sequence_id;
  report.type = type;
  report.reject_source = reject_source;
  report.reject_reason = reject_reason;

  reports_.push_back(report);
  ++reports_recorded_;

  if (reject_source != ExecutionRejectSource::kNone) {
    ++rejection_reports_;
  }

  if (IsActionReport(type)) {
    ++action_reports_;
  } else {
    ++order_reports_;
  }
}

}  // namespace llgw
