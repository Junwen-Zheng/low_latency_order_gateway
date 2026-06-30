#include "llgw/market_data_parser.hpp"

#include <charconv>
#include <cstdint>
#include <string_view>

namespace llgw {
namespace {

bool ParseDouble(std::string_view value, double* out) {
  if (value.empty() || out == nullptr) {
    return false;
  }

  const char* begin = value.data();
  const char* end = value.data() + value.size();

  auto [ptr, ec] = std::from_chars(begin, end, *out);
  return ec == std::errc{} && ptr == end;
}

bool ParseUint32(std::string_view value, std::uint32_t* out) {
  if (value.empty() || out == nullptr) {
    return false;
  }

  const char* begin = value.data();
  const char* end = value.data() + value.size();

  auto [ptr, ec] = std::from_chars(begin, end, *out);
  return ec == std::errc{} && ptr == end;
}

bool ParseUint64(std::string_view value, std::uint64_t* out) {
  if (value.empty() || out == nullptr) {
    return false;
  }

  const char* begin = value.data();
  const char* end = value.data() + value.size();

  auto [ptr, ec] = std::from_chars(begin, end, *out);
  return ec == std::errc{} && ptr == end;
}

bool NextField(std::string_view* input, std::string_view* field) {
  if (input == nullptr || field == nullptr) {
    return false;
  }

  const std::size_t pos = input->find(',');
  if (pos == std::string_view::npos) {
    *field = *input;
    *input = {};
    return true;
  }

  *field = input->substr(0, pos);
  input->remove_prefix(pos + 1);
  return true;
}

}  // namespace

ParseResult ParseMarketDataLine(std::string_view line) {
  ParseResult result{};

  if (line.empty()) {
    result.status = ParseStatus::kEmptyInput;
    return result;
  }

  MarketDataUpdate update{};
  std::string_view remaining = line;
  std::string_view field;

  if (!NextField(&remaining, &field) || field.empty()) {
    result.status = ParseStatus::kWrongFieldCount;
    return result;
  }
  update.symbol = field;

  if (!NextField(&remaining, &field) || !ParseDouble(field, &update.bid_price)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!NextField(&remaining, &field) || !ParseUint32(field, &update.bid_size)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!NextField(&remaining, &field) || !ParseDouble(field, &update.ask_price)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!NextField(&remaining, &field) || !ParseUint32(field, &update.ask_size)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!NextField(&remaining, &field) || !ParseUint64(field, &update.exchange_ts_ns)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!remaining.empty()) {
    result.status = ParseStatus::kWrongFieldCount;
    return result;
  }

  if (update.bid_price <= 0.0 || update.ask_price <= 0.0 || update.bid_price > update.ask_price) {
    result.status = ParseStatus::kInvalidMarket;
    return result;
  }

  result.status = ParseStatus::kOk;
  result.update = update;
  return result;
}

}  // namespace llgw
