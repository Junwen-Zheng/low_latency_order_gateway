#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <vector>

#include "llgw/market_data_parser.hpp"

namespace {

std::uint64_t ToNs(std::chrono::steady_clock::duration duration) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

std::uint64_t Percentile(const std::vector<std::uint64_t>& sorted_values,
                         std::size_t numerator,
                         std::size_t denominator) {
  if (sorted_values.empty()) {
    return 0;
  }

  const std::size_t index = ((sorted_values.size() - 1) * numerator) / denominator;
  return sorted_values[index];
}

}  // namespace

int main() {
  constexpr std::string_view line = "AAPL,192.10,100,192.12,200,1710000000123456789";

  constexpr std::size_t warmup_iterations = 10000;
  constexpr std::size_t measured_iterations = 100000;

  std::uint64_t checksum = 0;

  for (std::size_t i = 0; i < warmup_iterations; ++i) {
    const llgw::ParseResult result = llgw::ParseMarketDataLine(line);
    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Warmup parse failed\n";
      return 1;
    }

    checksum += result.update.bid_size;
  }

  std::vector<std::uint64_t> samples_ns;
  samples_ns.reserve(measured_iterations);

  const auto total_start = std::chrono::steady_clock::now();

  for (std::size_t i = 0; i < measured_iterations; ++i) {
    const auto start = std::chrono::steady_clock::now();
    const llgw::ParseResult result = llgw::ParseMarketDataLine(line);
    const auto end = std::chrono::steady_clock::now();

    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Measured parse failed\n";
      return 1;
    }

    checksum += result.update.bid_size;
    samples_ns.push_back(ToNs(end - start));
  }

  const auto total_end = std::chrono::steady_clock::now();

  std::sort(samples_ns.begin(), samples_ns.end());

  const std::uint64_t total_ns = ToNs(total_end - total_start);

  std::cout << "Parser benchmark smoke run\n";
  std::cout << "  warning: local measurement only, not a performance claim\n";
  std::cout << "  warmup_iterations=" << warmup_iterations << "\n";
  std::cout << "  measured_iterations=" << measured_iterations << "\n";
  std::cout << "  total_elapsed_ns=" << total_ns << "\n";
  std::cout << "  min_ns=" << samples_ns.front() << "\n";
  std::cout << "  p50_ns=" << Percentile(samples_ns, 50, 100) << "\n";
  std::cout << "  p95_ns=" << Percentile(samples_ns, 95, 100) << "\n";
  std::cout << "  p99_ns=" << Percentile(samples_ns, 99, 100) << "\n";
  std::cout << "  max_ns=" << samples_ns.back() << "\n";
  std::cout << "  checksum=" << checksum << "\n";

  return 0;
}
