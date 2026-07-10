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

struct BenchmarkInput {
  std::string name;
  std::string line;
};

struct BenchmarkConfig {
  std::size_t warmup_iterations = 10000;
  std::size_t measured_iterations = 100000;
  std::size_t trials = 5;
  std::string input_set = "single";
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
            << " [--warmup N] [--iterations N] [--trials N]"
            << " [--input-set single|varied] [--csv PATH]\n";
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
    } else if (arg == "--input-set") {
      const auto value = require_value(arg);
      if (!value.has_value()) {
        return false;
      }

      config->input_set = std::string(*value);
      if (config->input_set != "single" && config->input_set != "varied") {
        std::cerr << "Invalid --input-set value. Expected single or varied.\n";
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

std::vector<BenchmarkInput> BuildInputs(const std::string& input_set) {
  if (input_set == "single") {
    return {
        {"single_aapl", "AAPL,192.10,100,192.12,200,1710000000123456789"},
    };
  }

  return {
      {"aapl_tight", "AAPL,192.10,100,192.12,200,1710000000123456789"},
      {"msft_locked", "MSFT,420.50,300,420.50,100,1710000001123456789"},
      {"nvda_wide", "NVDA,905.25,50,905.75,75,1710000002123456789"},
      {"tsla_small_size", "TSLA,175.01,1,175.05,2,1710000003123456789"},
      {"amzn_large_size", "AMZN,185.10,10000,185.11,12000,1710000004123456789"},
      {"meta_decimal", "META,510.99,250,511.03,225,1710000005123456789"},
      {"goog_high_price", "GOOG,2890.12,10,2890.50,12,1710000006123456789"},
      {"spy_short_symbol", "SPY,540.01,500,540.02,600,1710000007123456789"},
  };
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

const char* OperatingSystem() {
#if defined(__APPLE__)
  return "apple";
#elif defined(__linux__)
  return "linux";
#elif defined(_WIN32)
  return "windows";
#else
  return "unknown";
#endif
}

const char* Architecture() {
#if defined(__aarch64__) || defined(_M_ARM64)
  return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#elif defined(__i386__) || defined(_M_IX86)
  return "x86";
#else
  return "unknown";
#endif
}

void PrintMetadata() {
  std::cout << "Benchmark metadata\n";
  std::cout << "  compiler=" << CompilerName() << "\n";
  std::cout << "  compiler_version=" << CompilerVersion() << "\n";
  std::cout << "  cplusplus=" << __cplusplus << "\n";
  std::cout << "  build_mode=" << BuildMode() << "\n";
  std::cout << "  operating_system=" << OperatingSystem() << "\n";
  std::cout << "  architecture=" << Architecture() << "\n";
  std::cout << "  steady_clock_is_steady="
            << (std::chrono::steady_clock::is_steady ? "true" : "false") << "\n";
  std::cout << "  hardware_concurrency=" << std::thread::hardware_concurrency() << "\n";
}

std::uint64_t UpdateChecksum(std::uint64_t checksum, const llgw::MarketDataUpdate& update) {
  checksum += update.bid_size;
  checksum += update.ask_size;
  checksum += static_cast<std::uint64_t>(update.symbol.size());
  checksum ^= update.exchange_ts_ns;
  return checksum;
}

TrialResult RunTrial(std::size_t trial_index,
                     const BenchmarkConfig& config,
                     const std::vector<BenchmarkInput>& inputs) {
  TrialResult trial{};
  trial.trial_index = trial_index;

  for (std::size_t i = 0; i < config.warmup_iterations; ++i) {
    const BenchmarkInput& input = inputs[i % inputs.size()];
    const llgw::ParseResult result = llgw::ParseMarketDataLine(input.line);
    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Warmup parse failed for input " << input.name << "\n";
      std::exit(1);
    }

    trial.checksum = UpdateChecksum(trial.checksum, result.update);
  }

  std::vector<std::uint64_t> samples_ns;
  samples_ns.reserve(config.measured_iterations);

  const auto total_start = std::chrono::steady_clock::now();

  for (std::size_t i = 0; i < config.measured_iterations; ++i) {
    const BenchmarkInput& input = inputs[i % inputs.size()];

    const auto start = std::chrono::steady_clock::now();
    const llgw::ParseResult result = llgw::ParseMarketDataLine(input.line);
    const auto end = std::chrono::steady_clock::now();

    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Measured parse failed for input " << input.name << "\n";
      std::exit(1);
    }

    trial.checksum = UpdateChecksum(trial.checksum, result.update);
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
              const std::vector<BenchmarkInput>& inputs,
              const std::vector<TrialResult>& trials) {
  std::ofstream file(path);
  if (!file) {
    std::cerr << "Failed to open CSV output: " << path << "\n";
    return false;
  }

  file << "trial,input_set,input_count,warmup_iterations,measured_iterations,"
          "total_elapsed_ns,min_ns,p50_ns,p95_ns,p99_ns,max_ns,checksum\n";

  for (const TrialResult& trial : trials) {
    file << trial.trial_index << ','
         << config.input_set << ','
         << inputs.size() << ','
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

  const std::vector<BenchmarkInput> inputs = BuildInputs(config.input_set);

  std::cout << "Parser benchmark smoke run\n";
  std::cout << "  warning: local measurement only, not a performance claim\n";
  std::cout << "  warmup_iterations=" << config.warmup_iterations << "\n";
  std::cout << "  measured_iterations=" << config.measured_iterations << "\n";
  std::cout << "  trials=" << config.trials << "\n";
  std::cout << "  input_set=" << config.input_set << "\n";
  std::cout << "  input_count=" << inputs.size() << "\n";

  PrintMetadata();

  std::vector<TrialResult> trials;
  trials.reserve(config.trials);

  for (std::size_t trial_index = 1; trial_index <= config.trials; ++trial_index) {
    TrialResult trial = RunTrial(trial_index, config, inputs);
    PrintTrial(trial);
    trials.push_back(trial);
  }

  if (config.csv_path.has_value()) {
    if (!WriteCsv(*config.csv_path, config, inputs, trials)) {
      return 1;
    }

    std::cout << "CSV written to " << *config.csv_path << "\n";
  }

  return 0;
}
