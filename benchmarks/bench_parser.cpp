#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "llgw/market_data_parser.hpp"

namespace {

struct BenchmarkConfig {
  std::size_t warmup_iterations = 10000;
  std::size_t measured_iterations = 100000;
  std::size_t trials = 5;
  std::optional<std::string> csv_path;
};

struct TrialResult {
  std::size_t trial_index = 0;
  std::uint64_t total_elapsed_ns = 0;
  std::uint64_t min_ns = 0;
  std::uint64_t p50_ns = 0;
  std::uint64_t p95_ns = 0;
  std::uint64_t p99_ns = 0;
  std::uint64_t max_ns = 0;
  std::uint64_t checksum = 0;
};

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

bool ParseSizeT(std::string_view value, std::size_t* output) {
  if (value.empty() || output == nullptr) {
    return false;
  }

  std::size_t parsed = 0;
  for (char ch : value) {
    if (ch < '0' || ch > '9') {
      return false;
    }

    parsed = parsed * 10 + static_cast<std::size_t>(ch - '0');
  }

  if (parsed == 0) {
    return false;
  }

  *output = parsed;
  return true;
}

void PrintUsage(const char* program) {
  std::cerr << "Usage: " << program
            << " [--warmup N] [--iterations N] [--trials N] [--csv PATH]\n";
}

bool ParseArgs(int argc, char** argv, BenchmarkConfig* config) {
  if (config == nullptr) {
    return false;
  }

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);

    auto require_value = [&](std::string_view flag) -> std::optional<std::string_view> {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        return std::nullopt;
      }

      ++i;
      return std::string_view(argv[i]);
    };

    if (arg == "--warmup") {
      const auto value = require_value(arg);
      if (!value.has_value() || !ParseSizeT(*value, &config->warmup_iterations)) {
        std::cerr << "Invalid --warmup value\n";
        return false;
      }
    } else if (arg == "--iterations") {
      const auto value = require_value(arg);
      if (!value.has_value() || !ParseSizeT(*value, &config->measured_iterations)) {
        std::cerr << "Invalid --iterations value\n";
        return false;
      }
    } else if (arg == "--trials") {
      const auto value = require_value(arg);
      if (!value.has_value() || !ParseSizeT(*value, &config->trials)) {
        std::cerr << "Invalid --trials value\n";
        return false;
      }
    } else if (arg == "--csv") {
      const auto value = require_value(arg);
      if (!value.has_value() || value->empty()) {
        std::cerr << "Invalid --csv value\n";
        return false;
      }

      config->csv_path = std::string(*value);
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      std::exit(0);
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      return false;
    }
  }

  return true;
}

const char* CompilerName() {
#if defined(__clang__)
  return "clang";
#elif defined(__GNUC__)
  return "gcc";
#elif defined(_MSC_VER)
  return "msvc";
#else
  return "unknown";
#endif
}

std::string CompilerVersion() {
  std::ostringstream oss;
#if defined(__clang__)
  oss << __clang_major__ << "." << __clang_minor__ << "." << __clang_patchlevel__;
#elif defined(__GNUC__)
  oss << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__;
#elif defined(_MSC_VER)
  oss << _MSC_VER;
#else
  oss << "unknown";
#endif
  return oss.str();
}

const char* BuildMode() {
#ifdef NDEBUG
  return "Release/NDEBUG";
#else
  return "Debug";
#endif
}

void PrintMetadata() {
  std::cout << "Benchmark metadata\n";
  std::cout << "  compiler=" << CompilerName() << "\n";
  std::cout << "  compiler_version=" << CompilerVersion() << "\n";
  std::cout << "  cplusplus=" << __cplusplus << "\n";
  std::cout << "  build_mode=" << BuildMode() << "\n";
  std::cout << "  steady_clock_is_steady="
            << (std::chrono::steady_clock::is_steady ? "true" : "false") << "\n";
  std::cout << "  hardware_concurrency=" << std::thread::hardware_concurrency() << "\n";
}

TrialResult RunTrial(std::size_t trial_index, const BenchmarkConfig& config) {
  constexpr std::string_view line = "AAPL,192.10,100,192.12,200,1710000000123456789";

  TrialResult trial{};
  trial.trial_index = trial_index;

  for (std::size_t i = 0; i < config.warmup_iterations; ++i) {
    const llgw::ParseResult result = llgw::ParseMarketDataLine(line);
    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Warmup parse failed\n";
      std::exit(1);
    }

    trial.checksum += result.update.bid_size;
  }

  std::vector<std::uint64_t> samples_ns;
  samples_ns.reserve(config.measured_iterations);

  const auto total_start = std::chrono::steady_clock::now();

  for (std::size_t i = 0; i < config.measured_iterations; ++i) {
    const auto start = std::chrono::steady_clock::now();
    const llgw::ParseResult result = llgw::ParseMarketDataLine(line);
    const auto end = std::chrono::steady_clock::now();

    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Measured parse failed\n";
      std::exit(1);
    }

    trial.checksum += result.update.bid_size;
    samples_ns.push_back(ToNs(end - start));
  }

  const auto total_end = std::chrono::steady_clock::now();

  std::sort(samples_ns.begin(), samples_ns.end());

  trial.total_elapsed_ns = ToNs(total_end - total_start);
  trial.min_ns = samples_ns.front();
  trial.p50_ns = Percentile(samples_ns, 50, 100);
  trial.p95_ns = Percentile(samples_ns, 95, 100);
  trial.p99_ns = Percentile(samples_ns, 99, 100);
  trial.max_ns = samples_ns.back();

  return trial;
}

void PrintTrial(const TrialResult& trial) {
  std::cout << "Trial " << trial.trial_index << "\n";
  std::cout << "  total_elapsed_ns=" << trial.total_elapsed_ns << "\n";
  std::cout << "  min_ns=" << trial.min_ns << "\n";
  std::cout << "  p50_ns=" << trial.p50_ns << "\n";
  std::cout << "  p95_ns=" << trial.p95_ns << "\n";
  std::cout << "  p99_ns=" << trial.p99_ns << "\n";
  std::cout << "  max_ns=" << trial.max_ns << "\n";
  std::cout << "  checksum=" << trial.checksum << "\n";
}

bool WriteCsv(const std::string& path,
              const BenchmarkConfig& config,
              const std::vector<TrialResult>& trials) {
  std::ofstream file(path);
  if (!file) {
    std::cerr << "Failed to open CSV output: " << path << "\n";
    return false;
  }

  file << "trial,warmup_iterations,measured_iterations,total_elapsed_ns,"
          "min_ns,p50_ns,p95_ns,p99_ns,max_ns,checksum\n";

  for (const TrialResult& trial : trials) {
    file << trial.trial_index << ','
         << config.warmup_iterations << ','
         << config.measured_iterations << ','
         << trial.total_elapsed_ns << ','
         << trial.min_ns << ','
         << trial.p50_ns << ','
         << trial.p95_ns << ','
         << trial.p99_ns << ','
         << trial.max_ns << ','
         << trial.checksum << '\n';
  }

  return true;
}

}  // namespace

int main(int argc, char** argv) {
  BenchmarkConfig config;
  if (!ParseArgs(argc, argv, &config)) {
    PrintUsage(argv[0]);
    return 1;
  }

  std::cout << "Parser benchmark smoke run\n";
  std::cout << "  warning: local measurement only, not a performance claim\n";
  std::cout << "  warmup_iterations=" << config.warmup_iterations << "\n";
  std::cout << "  measured_iterations=" << config.measured_iterations << "\n";
  std::cout << "  trials=" << config.trials << "\n";

  PrintMetadata();

  std::vector<TrialResult> trials;
  trials.reserve(config.trials);

  for (std::size_t trial_index = 1; trial_index <= config.trials; ++trial_index) {
    TrialResult trial = RunTrial(trial_index, config);
    PrintTrial(trial);
    trials.push_back(trial);
  }

  if (config.csv_path.has_value()) {
    if (!WriteCsv(*config.csv_path, config, trials)) {
      return 1;
    }

    std::cout << "CSV written to " << *config.csv_path << "\n";
  }

  return 0;
}
