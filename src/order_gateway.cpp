#include "llgw/order_gateway.hpp"

namespace llgw {

OrderGateway::OrderGateway(ExchangeSimulator* exchange) : exchange_(exchange) {}

OrderResponse OrderGateway::SendOrder(const OrderRequest& request) {
  ++orders_sent_;

  if (exchange_ == nullptr) {
    ++orders_rejected_;

    OrderResponse response{};
    response.sequence_id = request.sequence_id;
    response.accepted = false;
    response.reject_reason = OrderRejectReason::kInvalidSymbol;
    return response;
  }

  OrderResponse response = exchange_->SubmitOrder(request);

  if (response.accepted) {
    ++orders_accepted_;
  } else {
    ++orders_rejected_;
  }

  return response;
}

}  // namespace llgw
