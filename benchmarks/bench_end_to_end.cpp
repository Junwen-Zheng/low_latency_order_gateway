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
#include <utility>
#include <vector>

#include "llgw/exchange_simulator.hpp"
#include "llgw/market_data_parser.hpp"
#include "llgw/order_gateway.hpp"
#include "llgw/order_pipeline.hpp"
#include "llgw/simple_strategy.hpp"

namespace {

constexpr std::size_t kPipelineCapacity = 4096;
constexpr double kStrategyMinSpread = 0.02;

using Pipeline = llgw::OrderPipeline<kPipelineCapacity>;

struct BenchmarkInput {
  std::string name;
  std::string line;
};

struct BenchmarkConfig {
  std::size_t stabilization_iterations = 100000;
  std::size_t warmup_iterations = 6400;
  std::size_t measured_iterations = 64000;
  std::size_t trials = 5;
  std::size_t batch_size = 64;
  std::string workload = "mixed";
  std::optional<std::string> csv_path;
};

struct TimingDistribution {
  double min_ns_per_event = 0.0;
  double p50_ns_per_event = 0.0;
  double p95_ns_per_event = 0.0;
  double p99_ns_per_event = 0.0;
  double max_ns_per_event = 0.0;
};

struct PathCounters {
  std::uint64_t signals_seen = 0;
  std::uint64_t orders_generated = 0;
  std::uint64_t orders_enqueued = 0;
  std::uint64_t orders_dropped = 0;
  std::uint64_t orders_drained = 0;
  std::uint64_t pipeline_accepted = 0;
  std::uint64_t pipeline_rejected = 0;
  std::uint64_t gateway_sent = 0;
  std::uint64_t gateway_accepted = 0;
  std::uint64_t gateway_rejected = 0;
};

struct MeasurementResult {
  std::uint64_t total_elapsed_ns = 0;
  std::uint64_t checksum = 0;
  std::vector<double> samples_ns_per_event;
  PathCounters counters;
};

struct StabilizationResult {
  std::uint64_t elapsed_ns = 0;
  std::uint64_t checksum = 0;
  PathCounters counters;
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

  std::uint64_t path_total_elapsed_ns = 0;
  TimingDistribution path;

  double timed_events_per_second = 0.0;
  double orders_per_event = 0.0;

  PathCounters counters;
  std::uint64_t checksum = 0;
};

struct CrossTrialSummary {
  std::size_t trial_count = 0;
  double path_p50_median_ns_per_event = 0.0;
  double path_p95_median_ns_per_event = 0.0;
  double path_p99_median_ns_per_event = 0.0;
  double timed_events_per_second_median = 0.0;
  double orders_per_event_median = 0.0;
};

struct SystemState {
  llgw::ExchangeSimulator exchange;
  llgw::OrderGateway gateway;
  Pipeline pipeline;
  llgw::SimpleStrategy strategy;

  SystemState()
      : gateway(&exchange),
        pipeline(&gateway),
        strategy(kStrategyMinSpread) {}
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
  TimingDistribution result;

  if (samples.empty()) {
    return result;
  }

  std::sort(samples.begin(), samples.end());

  result.min_ns_per_event = samples.front();
  result.p50_ns_per_event = Percentile(samples, 50, 100);
  result.p95_ns_per_event = Percentile(samples, 95, 100);
  result.p99_ns_per_event = Percentile(samples, 99, 100);
  result.max_ns_per_event = samples.back();

  return result;
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
      << " [--stabilization-iterations N]"
      << " [--warmup N]"
      << " [--iterations N]"
      << " [--trials N]"
      << " [--batch-size N]"
      << " [--workload mixed|all-orders]"
      << " [--csv PATH]\n";
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

    if (arg == "--stabilization-iterations") {
      const auto value = require_value(arg);
      if (!value.has_value() ||
          !ParseSizeT(*value, &config->stabilization_iterations)) {
        std::cerr << "Invalid --stabilization-iterations value\n";
        return false;
      }
    } else if (arg == "--warmup") {
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
      if (!value.has_value() ||
          !ParseSizeT(*value, &config->trials)) {
        std::cerr << "Invalid --trials value\n";
        return false;
      }
    } else if (arg == "--batch-size") {
      const auto value = require_value(arg);
      if (!value.has_value() ||
          !ParseSizeT(*value, &config->batch_size)) {
        std::cerr << "Invalid --batch-size value\n";
        return false;
      }
    } else if (arg == "--workload") {
      const auto value = require_value(arg);
      if (!value.has_value()) {
        return false;
      }

      config->workload = std::string(*value);

      if (config->workload != "mixed" &&
          config->workload != "all-orders") {
        std::cerr
            << "Invalid --workload value. Expected mixed or all-orders.\n";
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

  if (config->batch_size > kPipelineCapacity) {
    std::cerr
        << "Batch size exceeds pipeline capacity of "
        << kPipelineCapacity << "\n";
    return false;
  }

  return true;
}

std::vector<BenchmarkInput> BuildInputs(const std::string& workload) {
  if (workload == "all-orders") {
    return {
        {"aapl_order",
         "AAPL,192.10,100,192.20,200,1710000000123456789"},
        {"msft_order",
         "MSFT,420.50,300,420.60,100,1710000001123456789"},
        {"nvda_order",
         "NVDA,905.25,50,905.40,75,1710000002123456789"},
        {"tsla_order",
         "TSLA,175.01,1,175.08,2,1710000003123456789"},
        {"amzn_order",
         "AMZN,185.10,10000,185.16,12000,1710000004123456789"},
        {"meta_order",
         "META,510.99,250,511.08,225,1710000005123456789"},
        {"goog_order",
         "GOOG,2890.12,10,2890.30,12,1710000006123456789"},
        {"spy_order",
         "SPY,540.01,500,540.07,600,1710000007123456789"},
    };
  }

  return {
      {"aapl_order",
       "AAPL,192.10,100,192.20,200,1710000000123456789"},
      {"msft_no_order",
       "MSFT,420.50,300,420.50,100,1710000001123456789"},
      {"nvda_order",
       "NVDA,905.25,50,905.40,75,1710000002123456789"},
      {"tsla_no_order",
       "TSLA,175.01,1,175.015,2,1710000003123456789"},
      {"amzn_no_order",
       "AMZN,185.10,10000,185.105,12000,1710000004123456789"},
      {"meta_order",
       "META,510.99,250,511.08,225,1710000005123456789"},
      {"goog_order",
       "GOOG,2890.12,10,2890.30,12,1710000006123456789"},
      {"spy_no_order",
       "SPY,540.01,500,540.015,600,1710000007123456789"},
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
  oss << __clang_major__ << "."
      << __clang_minor__ << "."
      << __clang_patchlevel__;
#elif defined(__GNUC__)
  oss << __GNUC__ << "."
      << __GNUC_MINOR__ << "."
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
            << (std::chrono::steady_clock::is_steady
                    ? "true"
                    : "false")
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

std::uint64_t ConsumeUpdate(
    std::uint64_t checksum,
    const llgw::MarketDataUpdate& update) {
  checksum += update.bid_size;
  checksum += update.ask_size;
  checksum += static_cast<std::uint64_t>(update.symbol.size());
  checksum ^= update.exchange_ts_ns;
  return checksum;
}

void ProcessEvents(SystemState* state,
                   const std::vector<BenchmarkInput>& inputs,
                   std::size_t start_index,
                   std::size_t count,
                   std::uint64_t* checksum) {
  if (state == nullptr || checksum == nullptr) {
    std::cerr << "Invalid benchmark state\n";
    std::exit(1);
  }

  for (std::size_t i = 0; i < count; ++i) {
    const BenchmarkInput& input =
        inputs[(start_index + i) % inputs.size()];

    const llgw::ParseResult parse_result =
        llgw::ParseMarketDataLine(input.line);

    if (parse_result.status != llgw::ParseStatus::kOk) {
      std::cerr
          << "Parse failed for benchmark input "
          << input.name << "\n";
      std::exit(1);
    }

    *checksum = ConsumeUpdate(
        *checksum,
        parse_result.update);

    const std::optional<llgw::OrderRequest> maybe_order =
        state->strategy.OnMarketData(parse_result.update);

    if (!maybe_order.has_value()) {
      continue;
    }

    if (!state->pipeline.Enqueue(*maybe_order)) {
      std::cerr
          << "Unexpected pipeline drop during benchmark\n";
      std::exit(1);
    }

    *checksum += maybe_order->sequence_id;
    *checksum += maybe_order->quantity;
    *checksum += maybe_order->symbol.size();
  }

  const std::size_t drained = state->pipeline.Drain();
  *checksum ^= static_cast<std::uint64_t>(drained);
}

PathCounters CaptureCounters(const SystemState& state) {
  PathCounters counters;

  counters.signals_seen =
      state.strategy.signals_seen();
  counters.orders_generated =
      state.strategy.orders_generated();

  counters.orders_enqueued =
      state.pipeline.orders_enqueued();
  counters.orders_dropped =
      state.pipeline.orders_dropped_on_enqueue();
  counters.orders_drained =
      state.pipeline.orders_drained();
  counters.pipeline_accepted =
      state.pipeline.orders_accepted();
  counters.pipeline_rejected =
      state.pipeline.orders_rejected();

  counters.gateway_sent =
      state.gateway.orders_sent();
  counters.gateway_accepted =
      state.gateway.orders_accepted();
  counters.gateway_rejected =
      state.gateway.orders_rejected();

  return counters;
}

void ValidateCounters(const PathCounters& counters,
                      std::size_t expected_events) {
  if (counters.signals_seen != expected_events) {
    std::cerr << "Unexpected signal count\n";
    std::exit(1);
  }

  if (counters.orders_generated !=
      counters.orders_enqueued + counters.orders_dropped) {
    std::cerr << "Generated/enqueued counter mismatch\n";
    std::exit(1);
  }

  if (counters.orders_dropped != 0) {
    std::cerr << "Benchmark unexpectedly dropped orders\n";
    std::exit(1);
  }

  if (counters.orders_drained != counters.orders_enqueued) {
    std::cerr << "Pipeline drain counter mismatch\n";
    std::exit(1);
  }

  if (counters.pipeline_accepted != counters.orders_drained ||
      counters.pipeline_rejected != 0) {
    std::cerr << "Pipeline response counter mismatch\n";
    std::exit(1);
  }

  if (counters.gateway_sent != counters.orders_drained ||
      counters.gateway_accepted != counters.gateway_sent ||
      counters.gateway_rejected != 0) {
    std::cerr << "Gateway counter mismatch\n";
    std::exit(1);
  }
}

StabilizationResult RunStabilization(
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs) {
  StabilizationResult result;
  SystemState state;

  const auto start = std::chrono::steady_clock::now();

  std::size_t event_index = 0;

  while (event_index < config.stabilization_iterations) {
    const std::size_t count =
        std::min(
            config.batch_size,
            config.stabilization_iterations - event_index);

    ProcessEvents(
        &state,
        inputs,
        event_index,
        count,
        &result.checksum);

    event_index += count;
  }

  const auto end = std::chrono::steady_clock::now();

  result.elapsed_ns = ToNs(end - start);
  result.counters = CaptureCounters(state);

  ValidateCounters(
      result.counters,
      config.stabilization_iterations);

  return result;
}

std::uint64_t RunWarmup(
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs) {
  SystemState state;
  std::uint64_t checksum = 0;
  std::size_t event_index = 0;

  while (event_index < config.warmup_iterations) {
    const std::size_t count =
        std::min(
            config.batch_size,
            config.warmup_iterations - event_index);

    ProcessEvents(
        &state,
        inputs,
        event_index,
        count,
        &checksum);

    event_index += count;
  }

  ValidateCounters(
      CaptureCounters(state),
      config.warmup_iterations);

  return checksum;
}

MeasurementResult MeasureBaseline(
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs) {
  MeasurementResult result;

  result.samples_ns_per_event.reserve(
      BatchCount(
          config.measured_iterations,
          config.batch_size));

  std::size_t event_index = 0;

  while (event_index < config.measured_iterations) {
    const std::size_t count =
        std::min(
            config.batch_size,
            config.measured_iterations - event_index);

    const auto start = std::chrono::steady_clock::now();

    for (std::size_t i = 0; i < count; ++i) {
      const BenchmarkInput& input =
          inputs[(event_index + i) % inputs.size()];

      result.checksum += input.line.size();
      result.checksum += input.name.size();
      result.checksum ^=
          static_cast<std::uint64_t>(
              static_cast<unsigned char>(
                  input.line.front()));
    }

    const auto end = std::chrono::steady_clock::now();
    const std::uint64_t elapsed_ns = ToNs(end - start);

    result.total_elapsed_ns += elapsed_ns;
    result.samples_ns_per_event.push_back(
        static_cast<double>(elapsed_ns) /
        static_cast<double>(count));

    event_index += count;
  }

  return result;
}

MeasurementResult MeasurePath(
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs) {
  MeasurementResult result;
  SystemState state;

  result.samples_ns_per_event.reserve(
      BatchCount(
          config.measured_iterations,
          config.batch_size));

  std::size_t event_index = 0;

  while (event_index < config.measured_iterations) {
    const std::size_t count =
        std::min(
            config.batch_size,
            config.measured_iterations - event_index);

    const auto start = std::chrono::steady_clock::now();

    ProcessEvents(
        &state,
        inputs,
        event_index,
        count,
        &result.checksum);

    const auto end = std::chrono::steady_clock::now();
    const std::uint64_t elapsed_ns = ToNs(end - start);

    result.total_elapsed_ns += elapsed_ns;
    result.samples_ns_per_event.push_back(
        static_cast<double>(elapsed_ns) /
        static_cast<double>(count));

    event_index += count;
  }

  result.counters = CaptureCounters(state);

  ValidateCounters(
      result.counters,
      config.measured_iterations);

  return result;
}

const char* TrialPhase(std::size_t trial_index) {
  return trial_index == 1
             ? "first_measured"
             : "steady_state_candidate";
}

TrialResult RunTrial(
    std::size_t trial_index,
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs) {
  TrialResult trial;

  trial.trial_index = trial_index;
  trial.timed_batches =
      BatchCount(
          config.measured_iterations,
          config.batch_size);

  const std::uint64_t warmup_checksum =
      RunWarmup(config, inputs);

  MeasurementResult baseline;
  MeasurementResult path;

  if (trial_index % 2 == 1) {
    baseline = MeasureBaseline(config, inputs);
    path = MeasurePath(config, inputs);
  } else {
    path = MeasurePath(config, inputs);
    baseline = MeasureBaseline(config, inputs);
  }

  trial.baseline_total_elapsed_ns =
      baseline.total_elapsed_ns;
  trial.baseline =
      Summarize(
          std::move(baseline.samples_ns_per_event));

  trial.path_total_elapsed_ns =
      path.total_elapsed_ns;
  trial.path =
      Summarize(
          std::move(path.samples_ns_per_event));

  trial.counters = path.counters;

  if (trial.path_total_elapsed_ns > 0) {
    trial.timed_events_per_second =
        static_cast<double>(
            config.measured_iterations) *
        1'000'000'000.0 /
        static_cast<double>(
            trial.path_total_elapsed_ns);
  }

  if (trial.counters.signals_seen > 0) {
    trial.orders_per_event =
        static_cast<double>(
            trial.counters.orders_generated) /
        static_cast<double>(
            trial.counters.signals_seen);
  }

  trial.checksum =
      warmup_checksum ^
      baseline.checksum ^
      path.checksum;

  return trial;
}

void PrintDistribution(
    const char* prefix,
    const TimingDistribution& distribution) {
  std::cout << "  " << prefix << "_min_ns_per_event="
            << distribution.min_ns_per_event << "\n";
  std::cout << "  " << prefix << "_p50_ns_per_event="
            << distribution.p50_ns_per_event << "\n";
  std::cout << "  " << prefix << "_p95_ns_per_event="
            << distribution.p95_ns_per_event << "\n";
  std::cout << "  " << prefix << "_p99_ns_per_event="
            << distribution.p99_ns_per_event << "\n";
  std::cout << "  " << prefix << "_max_ns_per_event="
            << distribution.max_ns_per_event << "\n";
}

void PrintCounters(const PathCounters& counters) {
  std::cout << "  signals_seen="
            << counters.signals_seen << "\n";
  std::cout << "  orders_generated="
            << counters.orders_generated << "\n";
  std::cout << "  orders_enqueued="
            << counters.orders_enqueued << "\n";
  std::cout << "  orders_dropped="
            << counters.orders_dropped << "\n";
  std::cout << "  orders_drained="
            << counters.orders_drained << "\n";
  std::cout << "  pipeline_accepted="
            << counters.pipeline_accepted << "\n";
  std::cout << "  pipeline_rejected="
            << counters.pipeline_rejected << "\n";
  std::cout << "  gateway_sent="
            << counters.gateway_sent << "\n";
  std::cout << "  gateway_accepted="
            << counters.gateway_accepted << "\n";
  std::cout << "  gateway_rejected="
            << counters.gateway_rejected << "\n";
}

void PrintTrial(const TrialResult& trial) {
  std::cout << "Trial " << trial.trial_index << "\n";
  std::cout << "  trial_phase="
            << TrialPhase(trial.trial_index) << "\n";
  std::cout << "  timed_batches="
            << trial.timed_batches << "\n";

  std::cout << "  baseline_total_elapsed_ns="
            << trial.baseline_total_elapsed_ns << "\n";
  PrintDistribution("baseline", trial.baseline);

  std::cout << "  path_total_elapsed_ns="
            << trial.path_total_elapsed_ns << "\n";
  PrintDistribution("path", trial.path);

  std::cout << "  timed_events_per_second="
            << trial.timed_events_per_second << "\n";
  std::cout << "  orders_per_event="
            << trial.orders_per_event << "\n";

  PrintCounters(trial.counters);

  std::cout << "  checksum="
            << trial.checksum << "\n";
}

CrossTrialSummary BuildCrossTrialSummary(
    const std::vector<TrialResult>& trials,
    std::size_t first_index) {
  CrossTrialSummary summary;

  if (first_index >= trials.size()) {
    return summary;
  }

  std::vector<double> p50_values;
  std::vector<double> p95_values;
  std::vector<double> p99_values;
  std::vector<double> throughput_values;
  std::vector<double> order_rate_values;

  for (std::size_t i = first_index;
       i < trials.size();
       ++i) {
    p50_values.push_back(
        trials[i].path.p50_ns_per_event);
    p95_values.push_back(
        trials[i].path.p95_ns_per_event);
    p99_values.push_back(
        trials[i].path.p99_ns_per_event);
    throughput_values.push_back(
        trials[i].timed_events_per_second);
    order_rate_values.push_back(
        trials[i].orders_per_event);
  }

  std::sort(p50_values.begin(), p50_values.end());
  std::sort(p95_values.begin(), p95_values.end());
  std::sort(p99_values.begin(), p99_values.end());
  std::sort(
      throughput_values.begin(),
      throughput_values.end());
  std::sort(
      order_rate_values.begin(),
      order_rate_values.end());

  summary.trial_count = p50_values.size();
  summary.path_p50_median_ns_per_event =
      Percentile(p50_values, 50, 100);
  summary.path_p95_median_ns_per_event =
      Percentile(p95_values, 50, 100);
  summary.path_p99_median_ns_per_event =
      Percentile(p99_values, 50, 100);
  summary.timed_events_per_second_median =
      Percentile(throughput_values, 50, 100);
  summary.orders_per_event_median =
      Percentile(order_rate_values, 50, 100);

  return summary;
}

void PrintCrossTrialSummary(
    const char* name,
    const CrossTrialSummary& summary) {
  std::cout << name << "\n";
  std::cout << "  trial_count="
            << summary.trial_count << "\n";
  std::cout << "  path_p50_median_ns_per_event="
            << summary.path_p50_median_ns_per_event << "\n";
  std::cout << "  path_p95_median_ns_per_event="
            << summary.path_p95_median_ns_per_event << "\n";
  std::cout << "  path_p99_median_ns_per_event="
            << summary.path_p99_median_ns_per_event << "\n";
  std::cout << "  timed_events_per_second_median="
            << summary.timed_events_per_second_median << "\n";
  std::cout << "  orders_per_event_median="
            << summary.orders_per_event_median << "\n";
}

bool WriteCsv(
    const std::string& path,
    const BenchmarkConfig& config,
    const std::vector<BenchmarkInput>& inputs,
    const std::vector<TrialResult>& trials) {
  std::ofstream file(path);

  if (!file) {
    std::cerr
        << "Failed to open CSV output: "
        << path << "\n";
    return false;
  }

  file << std::fixed << std::setprecision(3);

  file
      << "trial,trial_phase,workload,input_count,"
      << "stabilization_iterations,warmup_iterations,"
      << "measured_iterations,batch_size,timed_batches,"
      << "baseline_total_elapsed_ns,"
      << "baseline_min_ns_per_event,"
      << "baseline_p50_ns_per_event,"
      << "baseline_p95_ns_per_event,"
      << "baseline_p99_ns_per_event,"
      << "baseline_max_ns_per_event,"
      << "path_total_elapsed_ns,"
      << "path_min_ns_per_event,"
      << "path_p50_ns_per_event,"
      << "path_p95_ns_per_event,"
      << "path_p99_ns_per_event,"
      << "path_max_ns_per_event,"
      << "timed_events_per_second,"
      << "orders_per_event,"
      << "signals_seen,orders_generated,"
      << "orders_enqueued,orders_dropped,"
      << "orders_drained,pipeline_accepted,"
      << "pipeline_rejected,gateway_sent,"
      << "gateway_accepted,gateway_rejected,"
      << "checksum\n";

  for (const TrialResult& trial : trials) {
    file
        << trial.trial_index << ','
        << TrialPhase(trial.trial_index) << ','
        << config.workload << ','
        << inputs.size() << ','
        << config.stabilization_iterations << ','
        << config.warmup_iterations << ','
        << config.measured_iterations << ','
        << config.batch_size << ','
        << trial.timed_batches << ','
        << trial.baseline_total_elapsed_ns << ','
        << trial.baseline.min_ns_per_event << ','
        << trial.baseline.p50_ns_per_event << ','
        << trial.baseline.p95_ns_per_event << ','
        << trial.baseline.p99_ns_per_event << ','
        << trial.baseline.max_ns_per_event << ','
        << trial.path_total_elapsed_ns << ','
        << trial.path.min_ns_per_event << ','
        << trial.path.p50_ns_per_event << ','
        << trial.path.p95_ns_per_event << ','
        << trial.path.p99_ns_per_event << ','
        << trial.path.max_ns_per_event << ','
        << trial.timed_events_per_second << ','
        << trial.orders_per_event << ','
        << trial.counters.signals_seen << ','
        << trial.counters.orders_generated << ','
        << trial.counters.orders_enqueued << ','
        << trial.counters.orders_dropped << ','
        << trial.counters.orders_drained << ','
        << trial.counters.pipeline_accepted << ','
        << trial.counters.pipeline_rejected << ','
        << trial.counters.gateway_sent << ','
        << trial.counters.gateway_accepted << ','
        << trial.counters.gateway_rejected << ','
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
      BuildInputs(config.workload);

  std::cout << std::fixed << std::setprecision(3);

  std::cout << "In-memory end-to-end benchmark smoke run\n";
  std::cout
      << "  warning: local measurement only, not a performance claim\n";
  std::cout
      << "  measured_path=text_parser_to_exchange_simulator\n";
  std::cout
      << "  file_io_and_feed_replay_are_excluded\n";
  std::cout
      << "  baseline_is_reported_but_not_automatically_subtracted\n";
  std::cout
      << "  first_trial_is_reported_and_excluded_only_from_the_steady_state_summary\n";
  std::cout << "  stabilization_iterations="
            << config.stabilization_iterations << "\n";
  std::cout << "  warmup_iterations="
            << config.warmup_iterations << "\n";
  std::cout << "  measured_iterations="
            << config.measured_iterations << "\n";
  std::cout << "  trials="
            << config.trials << "\n";
  std::cout << "  batch_size="
            << config.batch_size << "\n";
  std::cout << "  pipeline_capacity="
            << kPipelineCapacity << "\n";
  std::cout << "  workload="
            << config.workload << "\n";
  std::cout << "  input_count="
            << inputs.size() << "\n";

  const StabilizationResult stabilization =
      RunStabilization(config, inputs);

  std::cout << "Process stabilization\n";
  std::cout << "  stabilization_elapsed_ns="
            << stabilization.elapsed_ns << "\n";
  std::cout << "  stabilization_checksum="
            << stabilization.checksum << "\n";
  PrintCounters(stabilization.counters);

  const ClockPairResult clock_pair =
      MeasureClockPairOverhead(10000);
  PrintMetadata(clock_pair);

  std::vector<TrialResult> trials;
  trials.reserve(config.trials);

  for (std::size_t trial_index = 1;
       trial_index <= config.trials;
       ++trial_index) {
    TrialResult trial =
        RunTrial(
            trial_index,
            config,
            inputs);

    PrintTrial(trial);
    trials.push_back(trial);
  }

  const CrossTrialSummary all_trials_summary =
      BuildCrossTrialSummary(trials, 0);

  PrintCrossTrialSummary(
      "All-trial descriptive summary",
      all_trials_summary);

  if (trials.size() > 1) {
    const CrossTrialSummary steady_state_summary =
        BuildCrossTrialSummary(trials, 1);

    PrintCrossTrialSummary(
        "Steady-state candidate summary (first trial excluded)",
        steady_state_summary);
  } else {
    std::cout
        << "Steady-state candidate summary unavailable: more than one trial is required\n";
  }

  if (config.csv_path.has_value()) {
    if (!WriteCsv(
            *config.csv_path,
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
