#include "llgw/order_gateway.hpp"

#include "llgw/order_lifecycle.hpp"

namespace llgw {

OrderGateway::OrderGateway(
    ExchangeSimulator* exchange,
    OrderLifecycleTracker* lifecycle_tracker)
    : exchange_(exchange),
      lifecycle_tracker_(lifecycle_tracker) {}

OrderResponse OrderGateway::SendOrder(
    const OrderRequest& request) {
  ++orders_sent_;

  if (exchange_ == nullptr) {
    ++orders_rejected_;

    OrderResponse response{};
    response.sequence_id = request.sequence_id;
    response.accepted = false;
    response.reject_reason =
        OrderRejectReason::kInvalidSymbol;
    return response;
  }

  OrderResponse response =
      exchange_->SubmitOrder(request);

  if (response.accepted) {
    ++orders_accepted_;
  } else {
    ++orders_rejected_;
  }

  return response;
}

CancelResponse OrderGateway::SendCancel(
    const CancelRequest& request) {
  ++cancels_sent_;

  if (lifecycle_tracker_ != nullptr &&
      !lifecycle_tracker_->MarkCancelPending(
          request.sequence_id)) {
    ++lifecycle_transition_errors_;
    ++cancels_rejected_;

    CancelResponse response{};
    response.sequence_id = request.sequence_id;
    response.accepted = false;
    response.reject_reason =
        CancelRejectReason::kInvalidState;
    return response;
  }

  CancelResponse response{};

  if (exchange_ == nullptr) {
    response.sequence_id = request.sequence_id;
    response.accepted = false;
    response.reject_reason =
        CancelRejectReason::kUnknownOrder;
  } else {
    response = exchange_->CancelOrder(request);
  }

  if (response.accepted) {
    ++cancels_accepted_;

    if (lifecycle_tracker_ != nullptr &&
        !lifecycle_tracker_->MarkCancelled(
            request.sequence_id)) {
      ++lifecycle_transition_errors_;
    }
  } else {
    ++cancels_rejected_;

    if (lifecycle_tracker_ != nullptr &&
        !lifecycle_tracker_->MarkCancelRejected(
            request.sequence_id,
            response.reject_reason)) {
      ++lifecycle_transition_errors_;
    }
  }

  return response;
}

AmendResponse OrderGateway::SendAmend(
    const AmendRequest& request) {
  ++amends_sent_;

  if (lifecycle_tracker_ != nullptr &&
      !lifecycle_tracker_->MarkAmendPending(
          request.sequence_id)) {
    ++lifecycle_transition_errors_;
    ++amends_rejected_;

    AmendResponse response{};
    response.sequence_id = request.sequence_id;
    response.accepted = false;
    response.reject_reason =
        AmendRejectReason::kInvalidState;
    return response;
  }

  AmendResponse response{};

  if (exchange_ == nullptr) {
    response.sequence_id = request.sequence_id;
    response.accepted = false;
    response.reject_reason =
        AmendRejectReason::kUnknownOrder;
  } else {
    response = exchange_->AmendOrder(request);
  }

  if (response.accepted) {
    ++amends_accepted_;

    if (lifecycle_tracker_ != nullptr &&
        !lifecycle_tracker_->MarkAmendAccepted(
            request.sequence_id)) {
      ++lifecycle_transition_errors_;
    }
  } else {
    ++amends_rejected_;

    if (lifecycle_tracker_ != nullptr &&
        !lifecycle_tracker_->MarkAmendRejected(
            request.sequence_id,
            response.reject_reason)) {
      ++lifecycle_transition_errors_;
    }
  }

  return response;
}

}  // namespace llgw
