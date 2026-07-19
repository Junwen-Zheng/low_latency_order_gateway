#include "llgw/exchange_simulator.hpp"

#include <cmath>

namespace llgw {

OrderResponse ExchangeSimulator::SubmitOrder(
    const OrderRequest& request) {
  OrderResponse response{};
  response.sequence_id = request.sequence_id;

  if (request.symbol.empty()) {
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kInvalidSymbol;
    return response;
  }

  if (!std::isfinite(request.price) || request.price <= 0.0) {
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kInvalidPrice;
    return response;
  }

  if (request.quantity == 0) {
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kInvalidQuantity;
    return response;
  }

  const auto insert_result =
      seen_sequence_ids_.insert(request.sequence_id);

  if (!insert_result.second) {
    response.accepted = false;
    response.reject_reason =
        OrderRejectReason::kDuplicateSequence;
    return response;
  }

  active_orders_.emplace(request.sequence_id, request);

  response.accepted = true;
  response.reject_reason = OrderRejectReason::kNone;
  return response;
}

CancelResponse ExchangeSimulator::CancelOrder(
    const CancelRequest& request) {
  CancelResponse response{};
  response.sequence_id = request.sequence_id;

  const auto it = active_orders_.find(request.sequence_id);

  if (it == active_orders_.end()) {
    response.accepted = false;
    response.reject_reason =
        CancelRejectReason::kUnknownOrder;
    return response;
  }

  active_orders_.erase(it);

  response.accepted = true;
  response.reject_reason = CancelRejectReason::kNone;
  return response;
}

AmendResponse ExchangeSimulator::AmendOrder(
    const AmendRequest& request) {
  AmendResponse response{};
  response.sequence_id = request.sequence_id;

  const auto it = active_orders_.find(request.sequence_id);

  if (it == active_orders_.end()) {
    response.accepted = false;
    response.reject_reason =
        AmendRejectReason::kUnknownOrder;
    return response;
  }

  if (!std::isfinite(request.new_price) ||
      request.new_price <= 0.0) {
    response.accepted = false;
    response.reject_reason =
        AmendRejectReason::kInvalidPrice;
    return response;
  }

  if (request.new_quantity == 0) {
    response.accepted = false;
    response.reject_reason =
        AmendRejectReason::kInvalidQuantity;
    return response;
  }

  it->second.price = request.new_price;
  it->second.quantity = request.new_quantity;

  response.accepted = true;
  response.reject_reason = AmendRejectReason::kNone;
  return response;
}

bool ExchangeSimulator::HasActiveOrder(
    std::uint64_t sequence_id) const {
  return active_orders_.find(sequence_id) !=
         active_orders_.end();
}

std::optional<OrderRequest>
ExchangeSimulator::FindActiveOrder(
    std::uint64_t sequence_id) const {
  const auto it = active_orders_.find(sequence_id);

  if (it == active_orders_.end()) {
    return std::nullopt;
  }

  return it->second;
}

}  // namespace llgw
