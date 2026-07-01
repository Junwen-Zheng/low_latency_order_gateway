#include "llgw/market_data_parser.hpp"

#include <array>
#include <charconv>
#include <cstdint>
#include <string_view>

namespace llgw {
namespace {

constexpr std::size_t kExpectedFieldCount = 6;

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

std::string_view StripTrailingCarriageReturn(std::string_view line) {
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }
  return line;
}

bool SplitCsvLine(std::string_view line, std::array<std::string_view, kExpectedFieldCount>* fields) {
  if (fields == nullptr) {
    return false;
  }

  std::size_t field_index = 0;
  std::size_t start = 0;

  while (true) {
    if (field_index >= kExpectedFieldCount) {
      return false;
    }

    const std::size_t comma = line.find(',', start);
    if (comma == std::string_view::npos) {
      (*fields)[field_index++] = line.substr(start);
      break;
    }

    (*fields)[field_index++] = line.substr(start, comma - start);
    start = comma + 1;
  }

  return field_index == kExpectedFieldCount;
}

}  // namespace

ParseResult ParseMarketDataLine(std::string_view line) {
  ParseResult result{};
  line = StripTrailingCarriageReturn(line);

  if (line.empty()) {
    result.status = ParseStatus::kEmptyInput;
    return result;
  }

  std::array<std::string_view, kExpectedFieldCount> fields{};
  if (!SplitCsvLine(line, &fields)) {
    result.status = ParseStatus::kWrongFieldCount;
    return result;
  }

  MarketDataUpdate update{};
  update.symbol = fields[0];

  if (update.symbol.empty()) {
    result.status = ParseStatus::kWrongFieldCount;
    return result;
  }

  if (!ParseDouble(fields[1], &update.bid_price)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!ParseUint32(fields[2], &update.bid_size)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!ParseDouble(fields[3], &update.ask_price)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!ParseUint32(fields[4], &update.ask_size)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (!ParseUint64(fields[5], &update.exchange_ts_ns)) {
    result.status = ParseStatus::kInvalidNumber;
    return result;
  }

  if (update.bid_price <= 0.0 || update.ask_price <= 0.0 || update.bid_price > update.ask_price ||
      update.bid_size == 0 || update.ask_size == 0) {
    result.status = ParseStatus::kInvalidMarket;
    return result;
  }

  result.status = ParseStatus::kOk;
  result.update = update;
  return result;
}

}  // namespace llgw
