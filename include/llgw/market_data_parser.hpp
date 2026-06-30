#pragma once

#include <string_view>

#include "llgw/messages.hpp"

namespace llgw {

enum class ParseStatus {
  kOk,
  kEmptyInput,
  kWrongFieldCount,
  kInvalidNumber,
  kInvalidMarket,
};

struct ParseResult {
  ParseStatus status = ParseStatus::kInvalidNumber;
  MarketDataUpdate update{};
};

ParseResult ParseMarketDataLine(std::string_view line);

}  // namespace llgw
