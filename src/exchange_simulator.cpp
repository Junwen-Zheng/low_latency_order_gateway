#include "llgw/exchange_simulator.hpp"

namespace llgw {

OrderResponse ExchangeSimulator::SubmitOrder(const OrderRequest& request) {
  OrderResponse response{};
  response.sequence_id = request.sequence_id;

  if (request.symbol.empty()) {
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kInvalidSymbol;
    return response;
  }

  if (request.price <= 0.0) {
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kInvalidPrice;
    return response;
  }

  if (request.quantity == 0) {
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kInvalidQuantity;
    return response;
  }

  const auto [_, inserted] = seen_sequence_ids_.insert(request.sequence_id);
  if (!inserted) {
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kDuplicateSequence;
    return response;
  }

  response.accepted = true;
  response.reject_reason = OrderRejectReason::kNone;
  return response;
}

}  // namespace llgw
