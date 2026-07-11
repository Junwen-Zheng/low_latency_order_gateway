#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
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
  std::size_t batch_size = 64;
  std::string input_set = "single";
  std::optional<std::string> csv_path;
};

struct TimingDistribution {
  double min_ns_per_op = 0.0;
  double p50_ns_per_op = 0.0;
  double p95_ns_per_op = 0.0;
  double p99_ns_per_op = 0.0;
  double max_ns_per_op = 0.0;
};

struct MeasurementResult {
  std::uint64_t total_elapsed_ns = 0;
  std::uint64_t checksum = 0;
  std::vector<double> samples_ns_per_op;
};

struct ClockPairResult {
  std::uint64_t min_ns = 0;
  std::uint64_t p50_ns = 0;
  std::uint64_t p95_ns = 0;
  std::uint64_t p99_ns = 0;
  std::uint64_t max_ns = 0;
};

struct TrialResult {
  std::size_t trial_index = 0;
  std::size_t timed_batches = 0;

  std::uint64_t baseline_total_elapsed_ns = 0;
  TimingDistribution baseline;

  std::uint64_t parser_total_elapsed_ns = 0;
  TimingDistribution parser;

  std::uint64_t checksum = 0;
};

std::uint64_t ToNs(std::chrono::steady_clock::duration duration) {
  return static_cast<std::uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
}

std::size_t BatchCount(std::size_t operations, std::size_t batch_size) {
  return (operations + batch_size - 1) / batch_size;
}

double Percentile(const std::vector<double>& sorted_values,
                  std::size_t numerator,
                  std::size_t denominator) {
  if (sorted_values.empty()) {
    return 0.0;
  }

  const std::size_t index =
      ((sorted_values.size() - 1) * numerator) / denominator;
  return sorted_values[index];
}

std::uint64_t Percentile(const std::vector<std::uint64_t>& sorted_values,
                         std::size_t numerator,
                         std::size_t denominator) {
  if (sorted_values.empty()) {
    return 0;
  }

  const std::size_t index =
      ((sorted_values.size() - 1) * numerator) / denominator;
  return sorted_values[index];
}

TimingDistribution Summarize(std::vector<double> samples) {
  TimingDistribution distribution;

  if (samples.empty()) {
    return distribution;
  }

  std::sort(samples.begin(), samples.end());

  distribution.min_ns_per_op = samples.front();
  distribution.p50_ns_per_op = Percentile(samples, 50, 100);
  distribution.p95_ns_per_op = Percentile(samples, 95, 100);
  distribution.p99_ns_per_op = Percentile(samples, 99, 100);
  distribution.max_ns_per_op = samples.back();

  return distribution;
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
  std::cerr
      << "Usage: " << program
      << " [--warmup N] [--iterations N] [--trials N]"
      << " [--batch-size N] [--input-set single|varied] [--csv PATH]\n";
}

bool ParseArgs(int argc, char** argv, BenchmarkConfig* config) {
  if (config == nullptr) {
    return false;
  }

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);

    auto require_value =
        [&](std::string_view flag) -> std::optional<std::string_view> {
      if (i + 1 >= argc) {
        std::cerr << "Missing value for " << flag << "\n";
        return std::nullopt;
      }

      ++i;
      return std::string_view(argv[i]);
    };

    if (arg == "--warmup") {
      const auto value = require_value(arg);
      if (!value.has_value() ||
          !ParseSizeT(*value, &config->warmup_iterations)) {
        std::cerr << "Invalid --warmup value\n";
        return false;
      }
    } else if (arg == "--iterations") {
      const auto value = require_value(arg);
      if (!value.has_value() ||
          !ParseSizeT(*value, &config->measured_iterations)) {
        std::cerr << "Invalid --iterations value\n";
        return false;
      }
    } else if (arg == "--trials") {
      const auto value = require_value(arg);
      if (!value.has_value() || !ParseSizeT(*value, &config->trials)) {
        std::cerr << "Invalid --trials value\n";
        return false;
      }
    } else if (arg == "--batch-size") {
      const auto value = require_value(arg);
      if (!value.has_value() || !ParseSizeT(*value, &config->batch_size)) {
        std::cerr << "Invalid --batch-size value\n";
        return false;
      }
    } else if (arg == "--input-set") {
      const auto value = require_value(arg);
      if (!value.has_value()) {
        return false;
      }

      config->input_set = std::string(*value);

      if (config->input_set != "single" &&
          config->input_set != "varied") {
        std::cerr
            << "Invalid --input-set value. Expected single or varied.\n";
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
        {"single_aapl",
         "AAPL,192.10,100,192.12,200,1710000000123456789"},
    };
  }

  return {
      {"aapl_tight",
       "AAPL,192.10,100,192.12,200,1710000000123456789"},
      {"msft_locked",
       "MSFT,420.50,300,420.50,100,1710000001123456789"},
      {"nvda_wide",
       "NVDA,905.25,50,905.75,75,1710000002123456789"},
      {"tsla_small_size",
       "TSLA,175.01,1,175.05,2,1710000003123456789"},
      {"amzn_large_size",
       "AMZN,185.10,10000,185.11,12000,1710000004123456789"},
      {"meta_decimal",
       "META,510.99,250,511.03,225,1710000005123456789"},
      {"goog_high_price",
       "GOOG,2890.12,10,2890.50,12,1710000006123456789"},
      {"spy_short_symbol",
       "SPY,540.01,500,540.02,600,1710000007123456789"},
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
  oss << __clang_major__ << "." << __clang_minor__ << "."
      << __clang_patchlevel__;
#elif defined(__GNUC__)
  oss << __GNUC__ << "." << __GNUC_MINOR__ << "."
      << __GNUC_PATCHLEVEL__;
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

ClockPairResult MeasureClockPairOverhead(std::size_t sample_count) {
  std::vector<std::uint64_t> samples;
  samples.reserve(sample_count);

  for (std::size_t i = 0; i < sample_count; ++i) {
    const auto start = std::chrono::steady_clock::now();
    const auto end = std::chrono::steady_clock::now();
    samples.push_back(ToNs(end - start));
  }

  std::sort(samples.begin(), samples.end());

  ClockPairResult result;
  result.min_ns = samples.front();
  result.p50_ns = Percentile(samples, 50, 100);
  result.p95_ns = Percentile(samples, 95, 100);
  result.p99_ns = Percentile(samples, 99, 100);
  result.max_ns = samples.back();

  return result;
}

void PrintMetadata(const ClockPairResult& clock_pair) {
  std::cout << "Benchmark metadata\n";
  std::cout << "  compiler=" << CompilerName() << "\n";
  std::cout << "  compiler_version=" << CompilerVersion() << "\n";
  std::cout << "  cplusplus=" << __cplusplus << "\n";
  std::cout << "  build_mode=" << BuildMode() << "\n";
  std::cout << "  operating_system=" << OperatingSystem() << "\n";
  std::cout << "  architecture=" << Architecture() << "\n";
  std::cout << "  steady_clock_is_steady="
            << (std::chrono::steady_clock::is_steady ? "true" : "false")
            << "\n";
  std::cout << "  hardware_concurrency="
            << std::thread::hardware_concurrency() << "\n";
  std::cout << "Clock-pair measurement\n";
  std::cout << "  clock_pair_min_ns=" << clock_pair.min_ns << "\n";
  std::cout << "  clock_pair_p50_ns=" << clock_pair.p50_ns << "\n";
  std::cout << "  clock_pair_p95_ns=" << clock_pair.p95_ns << "\n";
  std::cout << "  clock_pair_p99_ns=" << clock_pair.p99_ns << "\n";
  std::cout << "  clock_pair_max_ns=" << clock_pair.max_ns << "\n";
}

std::uint64_t UpdateParserChecksum(
    std::uint64_t checksum,
    const llgw::MarketDataUpdate& update) {
  checksum += update.bid_size;
  checksum += update.ask_size;
  checksum += static_cast<std::uint64_t>(update.symbol.size());
  checksum ^= update.exchange_ts_ns;
  return checksum;
}

void RunWarmup(const BenchmarkConfig& config,
               const std::vector<BenchmarkInput>& inputs,
               std::uint64_t* checksum) {
  for (std::size_t i = 0; i < config.warmup_iterations; ++i) {
    const BenchmarkInput& input = inputs[i % inputs.size()];
    const llgw::ParseResult result =
        llgw::ParseMarketDataLine(input.line);

    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr << "Warmup parse failed for input "
                << input.name << "\n";
      std::exit(1);
    }

    *checksum = UpdateParserChecksum(*checksum, result.update);
  }
}

MeasurementResult MeasureBaseline(
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs) {
  MeasurementResult result;
  result.samples_ns_per_op.reserve(
      BatchCount(config.measured_iterations, config.batch_size));

  std::size_t operation_index = 0;

  while (operation_index < config.measured_iterations) {
    const std::size_t operations_in_batch =
        std::min(config.batch_size,
                 config.measured_iterations - operation_index);

    const auto start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < operations_in_batch; ++i) {
      const BenchmarkInput& input =
          inputs[(operation_index + i) % inputs.size()];

      result.checksum +=
          static_cast<std::uint64_t>(input.line.size());
      result.checksum ^=
          static_cast<std::uint64_t>(
              static_cast<unsigned char>(input.line.front()));
      result.checksum +=
          static_cast<std::uint64_t>(input.name.size());
    }

    const auto end = std::chrono::steady_clock::now();
    const std::uint64_t elapsed_ns = ToNs(end - start);

    result.total_elapsed_ns += elapsed_ns;
    result.samples_ns_per_op.push_back(
        static_cast<double>(elapsed_ns) /
        static_cast<double>(operations_in_batch));

    operation_index += operations_in_batch;
  }

  return result;
}

MeasurementResult MeasureParser(
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs) {
  MeasurementResult result;
  result.samples_ns_per_op.reserve(
      BatchCount(config.measured_iterations, config.batch_size));

  std::size_t operation_index = 0;

  while (operation_index < config.measured_iterations) {
    const std::size_t operations_in_batch =
        std::min(config.batch_size,
                 config.measured_iterations - operation_index);

    const auto start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < operations_in_batch; ++i) {
      const BenchmarkInput& input =
          inputs[(operation_index + i) % inputs.size()];

      const llgw::ParseResult parse_result =
          llgw::ParseMarketDataLine(input.line);

      if (parse_result.status != llgw::ParseStatus::kOk) {
        std::cerr << "Measured parse failed for input "
                  << input.name << "\n";
        std::exit(1);
      }

      result.checksum =
          UpdateParserChecksum(result.checksum,
                               parse_result.update);
    }

    const auto end = std::chrono::steady_clock::now();
    const std::uint64_t elapsed_ns = ToNs(end - start);

    result.total_elapsed_ns += elapsed_ns;
    result.samples_ns_per_op.push_back(
        static_cast<double>(elapsed_ns) /
        static_cast<double>(operations_in_batch));

    operation_index += operations_in_batch;
  }

  return result;
}

TrialResult RunTrial(std::size_t trial_index,
                     const BenchmarkConfig& config,
                     const std::vector<BenchmarkInput>& inputs) {
  TrialResult trial;
  trial.trial_index = trial_index;
  trial.timed_batches =
      BatchCount(config.measured_iterations, config.batch_size);

  std::uint64_t warmup_checksum = 0;
  RunWarmup(config, inputs, &warmup_checksum);

  MeasurementResult baseline;
  MeasurementResult parser;

  if (trial_index % 2 == 1) {
    baseline = MeasureBaseline(config, inputs);
    parser = MeasureParser(config, inputs);
  } else {
    parser = MeasureParser(config, inputs);
    baseline = MeasureBaseline(config, inputs);
  }

  trial.baseline_total_elapsed_ns =
      baseline.total_elapsed_ns;
  trial.baseline =
      Summarize(std::move(baseline.samples_ns_per_op));

  trial.parser_total_elapsed_ns =
      parser.total_elapsed_ns;
  trial.parser =
      Summarize(std::move(parser.samples_ns_per_op));

  trial.checksum =
      warmup_checksum ^ baseline.checksum ^ parser.checksum;

  return trial;
}

void PrintDistribution(const char* prefix,
                       const TimingDistribution& distribution) {
  std::cout << "  " << prefix << "_min_ns_per_op="
            << distribution.min_ns_per_op << "\n";
  std::cout << "  " << prefix << "_p50_ns_per_op="
            << distribution.p50_ns_per_op << "\n";
  std::cout << "  " << prefix << "_p95_ns_per_op="
            << distribution.p95_ns_per_op << "\n";
  std::cout << "  " << prefix << "_p99_ns_per_op="
            << distribution.p99_ns_per_op << "\n";
  std::cout << "  " << prefix << "_max_ns_per_op="
            << distribution.max_ns_per_op << "\n";
}

void PrintTrial(const TrialResult& trial) {
  std::cout << "Trial " << trial.trial_index << "\n";
  std::cout << "  timed_batches=" << trial.timed_batches << "\n";
  std::cout << "  baseline_total_elapsed_ns="
            << trial.baseline_total_elapsed_ns << "\n";
  PrintDistribution("baseline", trial.baseline);
  std::cout << "  parser_total_elapsed_ns="
            << trial.parser_total_elapsed_ns << "\n";
  PrintDistribution("parser", trial.parser);
  std::cout << "  checksum=" << trial.checksum << "\n";
}

bool WriteCsv(const std::string& path,
              const BenchmarkConfig& config,
              const std::vector<BenchmarkInput>& inputs,
              const std::vector<TrialResult>& trials) {
  std::ofstream file(path);

  if (!file) {
    std::cerr << "Failed to open CSV output: "
              << path << "\n";
    return false;
  }

  file << std::fixed << std::setprecision(3);

  file
      << "trial,input_set,input_count,warmup_iterations,"
      << "measured_iterations,batch_size,timed_batches,"
      << "baseline_total_elapsed_ns,"
      << "baseline_min_ns_per_op,baseline_p50_ns_per_op,"
      << "baseline_p95_ns_per_op,baseline_p99_ns_per_op,"
      << "baseline_max_ns_per_op,"
      << "parser_total_elapsed_ns,"
      << "parser_min_ns_per_op,parser_p50_ns_per_op,"
      << "parser_p95_ns_per_op,parser_p99_ns_per_op,"
      << "parser_max_ns_per_op,checksum\n";

  for (const TrialResult& trial : trials) {
    file
        << trial.trial_index << ','
        << config.input_set << ','
        << inputs.size() << ','
        << config.warmup_iterations << ','
        << config.measured_iterations << ','
        << config.batch_size << ','
        << trial.timed_batches << ','
        << trial.baseline_total_elapsed_ns << ','
        << trial.baseline.min_ns_per_op << ','
        << trial.baseline.p50_ns_per_op << ','
        << trial.baseline.p95_ns_per_op << ','
        << trial.baseline.p99_ns_per_op << ','
        << trial.baseline.max_ns_per_op << ','
        << trial.parser_total_elapsed_ns << ','
        << trial.parser.min_ns_per_op << ','
        << trial.parser.p50_ns_per_op << ','
        << trial.parser.p95_ns_per_op << ','
        << trial.parser.p99_ns_per_op << ','
        << trial.parser.max_ns_per_op << ','
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

  const std::vector<BenchmarkInput> inputs =
      BuildInputs(config.input_set);

  const ClockPairResult clock_pair =
      MeasureClockPairOverhead(10000);

  std::cout << std::fixed << std::setprecision(3);

  std::cout << "Parser benchmark smoke run\n";
  std::cout
      << "  warning: local measurement only, not a performance claim\n";
  std::cout << "  timing_mode=batched\n";
  std::cout
      << "  baseline_is_reported_but_not_automatically_subtracted\n";
  std::cout << "  warmup_iterations="
            << config.warmup_iterations << "\n";
  std::cout << "  measured_iterations="
            << config.measured_iterations << "\n";
  std::cout << "  trials=" << config.trials << "\n";
  std::cout << "  batch_size=" << config.batch_size << "\n";
  std::cout << "  input_set=" << config.input_set << "\n";
  std::cout << "  input_count=" << inputs.size() << "\n";

  PrintMetadata(clock_pair);

  std::vector<TrialResult> trials;
  trials.reserve(config.trials);

  for (std::size_t trial_index = 1;
       trial_index <= config.trials;
       ++trial_index) {
    TrialResult trial =
        RunTrial(trial_index, config, inputs);
    PrintTrial(trial);
    trials.push_back(trial);
  }

  if (config.csv_path.has_value()) {
    if (!WriteCsv(*config.csv_path,
                  config,
                  inputs,
                  trials)) {
      return 1;
    }

    std::cout << "CSV written to "
              << *config.csv_path << "\n";
  }

  return 0;
}
