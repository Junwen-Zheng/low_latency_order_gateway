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

enum class PathMode {
  kText,
  kPrepared,
};

struct RawInput {
  std::string name;
  std::string line;
  bool should_generate_order = false;
};

struct PreparedInput {
  std::string name;
  std::string line;
  std::string symbol;

  double bid_price = 0.0;
  std::uint32_t bid_size = 0;
  double ask_price = 0.0;
  std::uint32_t ask_size = 0;
  std::uint64_t exchange_ts_ns = 0;

  bool should_generate_order = false;

  llgw::MarketDataUpdate AsUpdate() const {
    llgw::MarketDataUpdate update{};
    update.symbol = symbol;
    update.bid_price = bid_price;
    update.bid_size = bid_size;
    update.ask_price = ask_price;
    update.ask_size = ask_size;
    update.exchange_ts_ns = exchange_ts_ns;
    return update;
  }
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

struct ModeMeasurement {
  std::uint64_t total_elapsed_ns = 0;
  TimingDistribution timing;
  double timed_events_per_second = 0.0;
  PathCounters counters;
  std::uint64_t checksum = 0;
};

struct StabilizationResult {
  std::uint64_t elapsed_ns = 0;
  PathCounters counters;
  std::uint64_t checksum = 0;
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

  ModeMeasurement text;
  ModeMeasurement prepared;

  double text_minus_prepared_p50_ns_per_event = 0.0;
  double text_minus_prepared_total_avg_ns_per_event = 0.0;
  double orders_per_event = 0.0;
};

struct CrossTrialSummary {
  std::size_t trial_count = 0;

  double text_p50_median_ns_per_event = 0.0;
  double text_p95_median_ns_per_event = 0.0;
  double text_p99_median_ns_per_event = 0.0;
  double text_events_per_second_median = 0.0;

  double prepared_p50_median_ns_per_event = 0.0;
  double prepared_p95_median_ns_per_event = 0.0;
  double prepared_p99_median_ns_per_event = 0.0;
  double prepared_events_per_second_median = 0.0;

  double paired_p50_delta_median_ns_per_event = 0.0;
  double paired_total_avg_delta_median_ns_per_event = 0.0;
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

std::uint64_t Percentile(
    const std::vector<std::uint64_t>& sorted_values,
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
      << " [--workload no-orders|mixed|all-orders]"
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

      if (config->workload != "no-orders" &&
          config->workload != "mixed" &&
          config->workload != "all-orders") {
        std::cerr
            << "Invalid --workload value. Expected no-orders, mixed, or all-orders.\n";
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

std::vector<RawInput> BuildRawInputs(const std::string& workload) {
  const bool no_orders = workload == "no-orders";
  const bool all_orders = workload == "all-orders";

  auto should_order = [&](std::size_t index) {
    if (all_orders) {
      return true;
    }

    if (no_orders) {
      return false;
    }

    return index % 2 == 0;
  };

  const bool order0 = should_order(0);
  const bool order1 = should_order(1);
  const bool order2 = should_order(2);
  const bool order3 = should_order(3);
  const bool order4 = should_order(4);
  const bool order5 = should_order(5);
  const bool order6 = should_order(6);
  const bool order7 = should_order(7);

  return {
      {"aapl",
       order0
           ? "AAPL,192.10,100,192.20,200,1710000000123456789"
           : "AAPL,192.10,100,192.11,200,1710000000123456789",
       order0},
      {"msft",
       order1
           ? "MSFT,420.50,300,420.60,100,1710000001123456789"
           : "MSFT,420.50,300,420.51,100,1710000001123456789",
       order1},
      {"nvda",
       order2
           ? "NVDA,905.25,50,905.35,75,1710000002123456789"
           : "NVDA,905.25,50,905.26,75,1710000002123456789",
       order2},
      {"tsla",
       order3
           ? "TSLA,175.01,1,175.11,2,1710000003123456789"
           : "TSLA,175.01,1,175.02,2,1710000003123456789",
       order3},
      {"amzn",
       order4
           ? "AMZN,185.10,10000,185.20,12000,1710000004123456789"
           : "AMZN,185.10,10000,185.11,12000,1710000004123456789",
       order4},
      {"meta",
       order5
           ? "META,510.99,250,511.09,225,1710000005123456789"
           : "META,510.99,250,511.00,225,1710000005123456789",
       order5},
      {"goog",
       order6
           ? "GOOG,2890.12,10,2890.22,12,1710000006123456789"
           : "GOOG,2890.12,10,2890.13,12,1710000006123456789",
       order6},
      {"spy",
       order7
           ? "SPY,540.01,500,540.11,600,1710000007123456789"
           : "SPY,540.01,500,540.02,600,1710000007123456789",
       order7},
  };
}

std::vector<PreparedInput> PrepareInputs(
    const std::vector<RawInput>& raw_inputs) {
  std::vector<PreparedInput> prepared;
  prepared.reserve(raw_inputs.size());

  for (const RawInput& raw : raw_inputs) {
    const llgw::ParseResult result =
        llgw::ParseMarketDataLine(raw.line);

    if (result.status != llgw::ParseStatus::kOk) {
      std::cerr
          << "Failed to prepare benchmark input "
          << raw.name << "\n";
      std::exit(1);
    }

    PreparedInput input;
    input.name = raw.name;
    input.line = raw.line;
    input.symbol = std::string(result.update.symbol);
    input.bid_price = result.update.bid_price;
    input.bid_size = result.update.bid_size;
    input.ask_price = result.update.ask_price;
    input.ask_size = result.update.ask_size;
    input.exchange_ts_ns = result.update.exchange_ts_ns;
    input.should_generate_order = raw.should_generate_order;

    prepared.push_back(std::move(input));
  }

  return prepared;
}

const char* PathName(PathMode mode) {
  return mode == PathMode::kText
             ? "text"
             : "prepared";
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
  std::cout << "  hardware_concurrency="
            << std::thread::hardware_concurrency() << "\n";
  std::cout << "  steady_clock_is_steady="
            << (std::chrono::steady_clock::is_steady
                    ? "true"
                    : "false")
            << "\n";

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

PathCounters SubtractCounters(
    const PathCounters& after,
    const PathCounters& before) {
  PathCounters delta;

  delta.signals_seen =
      after.signals_seen - before.signals_seen;
  delta.orders_generated =
      after.orders_generated - before.orders_generated;
  delta.orders_enqueued =
      after.orders_enqueued - before.orders_enqueued;
  delta.orders_dropped =
      after.orders_dropped - before.orders_dropped;
  delta.orders_drained =
      after.orders_drained - before.orders_drained;
  delta.pipeline_accepted =
      after.pipeline_accepted - before.pipeline_accepted;
  delta.pipeline_rejected =
      after.pipeline_rejected - before.pipeline_rejected;
  delta.gateway_sent =
      after.gateway_sent - before.gateway_sent;
  delta.gateway_accepted =
      after.gateway_accepted - before.gateway_accepted;
  delta.gateway_rejected =
      after.gateway_rejected - before.gateway_rejected;

  return delta;
}

std::uint64_t ExpectedOrders(
    const std::vector<PreparedInput>& inputs,
    std::size_t start_index,
    std::size_t count) {
  std::uint64_t expected = 0;

  for (std::size_t i = 0; i < count; ++i) {
    if (inputs[(start_index + i) % inputs.size()]
            .should_generate_order) {
      ++expected;
    }
  }

  return expected;
}

void ValidateCounters(
    const PathCounters& counters,
    std::size_t expected_events,
    std::uint64_t expected_orders) {
  if (counters.signals_seen != expected_events) {
    std::cerr << "Unexpected signal count\n";
    std::exit(1);
  }

  if (counters.orders_generated != expected_orders) {
    std::cerr << "Unexpected generated-order count\n";
    std::exit(1);
  }

  if (counters.orders_enqueued != expected_orders ||
      counters.orders_dropped != 0) {
    std::cerr << "Unexpected enqueue/drop counters\n";
    std::exit(1);
  }

  if (counters.orders_drained != expected_orders ||
      counters.pipeline_accepted != expected_orders ||
      counters.pipeline_rejected != 0) {
    std::cerr << "Unexpected pipeline counters\n";
    std::exit(1);
  }

  if (counters.gateway_sent != expected_orders ||
      counters.gateway_accepted != expected_orders ||
      counters.gateway_rejected != 0) {
    std::cerr << "Unexpected gateway counters\n";
    std::exit(1);
  }
}

void ProcessEvents(
    PathMode mode,
    SystemState* state,
    const std::vector<PreparedInput>& inputs,
    std::size_t start_index,
    std::size_t count,
    std::uint64_t* checksum) {
  if (state == nullptr || checksum == nullptr) {
    std::cerr << "Invalid benchmark state\n";
    std::exit(1);
  }

  for (std::size_t i = 0; i < count; ++i) {
    const PreparedInput& input =
        inputs[(start_index + i) % inputs.size()];

    llgw::MarketDataUpdate update{};

    if (mode == PathMode::kText) {
      const llgw::ParseResult result =
          llgw::ParseMarketDataLine(input.line);

      if (result.status != llgw::ParseStatus::kOk) {
        std::cerr
            << "Text-path parse failed for "
            << input.name << "\n";
        std::exit(1);
      }

      update = result.update;
    } else {
      update = input.AsUpdate();
    }

    *checksum = ConsumeUpdate(*checksum, update);

    const std::optional<llgw::OrderRequest> maybe_order =
        state->strategy.OnMarketData(update);

    if (!maybe_order.has_value()) {
      continue;
    }

    if (!state->pipeline.Enqueue(*maybe_order)) {
      std::cerr
          << "Unexpected pipeline drop in "
          << PathName(mode) << " path\n";
      std::exit(1);
    }

    *checksum += maybe_order->sequence_id;
    *checksum += maybe_order->quantity;
    *checksum += maybe_order->symbol.size();
  }

  const std::size_t drained = state->pipeline.Drain();
  *checksum ^= static_cast<std::uint64_t>(drained);
}

StabilizationResult RunStabilization(
    PathMode mode,
    const BenchmarkConfig& config,
    const std::vector<PreparedInput>& inputs) {
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
        mode,
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
      config.stabilization_iterations,
      ExpectedOrders(
          inputs,
          0,
          config.stabilization_iterations));

  return result;
}

ModeMeasurement MeasureMode(
    PathMode mode,
    const BenchmarkConfig& config,
    const std::vector<PreparedInput>& inputs) {
  ModeMeasurement measurement;
  SystemState state;
  std::uint64_t checksum = 0;

  std::size_t warmup_index = 0;

  while (warmup_index < config.warmup_iterations) {
    const std::size_t count =
        std::min(
            config.batch_size,
            config.warmup_iterations - warmup_index);

    ProcessEvents(
        mode,
        &state,
        inputs,
        warmup_index,
        count,
        &checksum);

    warmup_index += count;
  }

  const PathCounters before = CaptureCounters(state);

  ValidateCounters(
      before,
      config.warmup_iterations,
      ExpectedOrders(
          inputs,
          0,
          config.warmup_iterations));

  std::vector<double> samples_ns_per_event;
  samples_ns_per_event.reserve(
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
        mode,
        &state,
        inputs,
        event_index,
        count,
        &checksum);

    const auto end = std::chrono::steady_clock::now();
    const std::uint64_t elapsed_ns = ToNs(end - start);

    measurement.total_elapsed_ns += elapsed_ns;
    samples_ns_per_event.push_back(
        static_cast<double>(elapsed_ns) /
        static_cast<double>(count));

    event_index += count;
  }

  const PathCounters after = CaptureCounters(state);
  measurement.counters = SubtractCounters(after, before);

  ValidateCounters(
      measurement.counters,
      config.measured_iterations,
      ExpectedOrders(
          inputs,
          0,
          config.measured_iterations));

  measurement.timing =
      Summarize(std::move(samples_ns_per_event));

  if (measurement.total_elapsed_ns > 0) {
    measurement.timed_events_per_second =
        static_cast<double>(config.measured_iterations) *
        1'000'000'000.0 /
        static_cast<double>(measurement.total_elapsed_ns);
  }

  measurement.checksum = checksum;
  return measurement;
}

const char* TrialPhase(std::size_t trial_index) {
  return trial_index == 1
             ? "first_measured"
             : "steady_state_candidate";
}

TrialResult RunTrial(
    std::size_t trial_index,
    const BenchmarkConfig& config,
    const std::vector<PreparedInput>& inputs) {
  TrialResult trial;
  trial.trial_index = trial_index;
  trial.timed_batches =
      BatchCount(
          config.measured_iterations,
          config.batch_size);

  if (trial_index % 2 == 1) {
    trial.text =
        MeasureMode(PathMode::kText, config, inputs);
    trial.prepared =
        MeasureMode(PathMode::kPrepared, config, inputs);
  } else {
    trial.prepared =
        MeasureMode(PathMode::kPrepared, config, inputs);
    trial.text =
        MeasureMode(PathMode::kText, config, inputs);
  }

  trial.text_minus_prepared_p50_ns_per_event =
      trial.text.timing.p50_ns_per_event -
      trial.prepared.timing.p50_ns_per_event;

  const double text_average =
      static_cast<double>(trial.text.total_elapsed_ns) /
      static_cast<double>(config.measured_iterations);

  const double prepared_average =
      static_cast<double>(trial.prepared.total_elapsed_ns) /
      static_cast<double>(config.measured_iterations);

  trial.text_minus_prepared_total_avg_ns_per_event =
      text_average - prepared_average;

  trial.orders_per_event =
      static_cast<double>(
          trial.text.counters.orders_generated) /
      static_cast<double>(
          trial.text.counters.signals_seen);

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

void PrintModeMeasurement(
    const char* name,
    const ModeMeasurement& measurement) {
  std::cout << "  " << name << "_total_elapsed_ns="
            << measurement.total_elapsed_ns << "\n";

  PrintDistribution(name, measurement.timing);

  std::cout << "  " << name << "_timed_events_per_second="
            << measurement.timed_events_per_second << "\n";
  std::cout << "  " << name << "_signals_seen="
            << measurement.counters.signals_seen << "\n";
  std::cout << "  " << name << "_orders_generated="
            << measurement.counters.orders_generated << "\n";
  std::cout << "  " << name << "_orders_dropped="
            << measurement.counters.orders_dropped << "\n";
  std::cout << "  " << name << "_pipeline_rejected="
            << measurement.counters.pipeline_rejected << "\n";
  std::cout << "  " << name << "_gateway_rejected="
            << measurement.counters.gateway_rejected << "\n";
  std::cout << "  " << name << "_checksum="
            << measurement.checksum << "\n";
}

void PrintTrial(const TrialResult& trial) {
  std::cout << "Trial " << trial.trial_index << "\n";
  std::cout << "  trial_phase="
            << TrialPhase(trial.trial_index) << "\n";
  std::cout << "  timed_batches="
            << trial.timed_batches << "\n";

  PrintModeMeasurement("text", trial.text);
  PrintModeMeasurement("prepared", trial.prepared);

  std::cout
      << "  text_minus_prepared_p50_ns_per_event="
      << trial.text_minus_prepared_p50_ns_per_event
      << "\n";
  std::cout
      << "  text_minus_prepared_total_avg_ns_per_event="
      << trial.text_minus_prepared_total_avg_ns_per_event
      << "\n";
  std::cout << "  orders_per_event="
            << trial.orders_per_event << "\n";
}

CrossTrialSummary BuildCrossTrialSummary(
    const std::vector<TrialResult>& trials,
    std::size_t first_index) {
  CrossTrialSummary summary;

  if (first_index >= trials.size()) {
    return summary;
  }

  std::vector<double> text_p50;
  std::vector<double> text_p95;
  std::vector<double> text_p99;
  std::vector<double> text_throughput;

  std::vector<double> prepared_p50;
  std::vector<double> prepared_p95;
  std::vector<double> prepared_p99;
  std::vector<double> prepared_throughput;

  std::vector<double> paired_p50_delta;
  std::vector<double> paired_average_delta;
  std::vector<double> order_rate;

  for (std::size_t i = first_index;
       i < trials.size();
       ++i) {
    text_p50.push_back(
        trials[i].text.timing.p50_ns_per_event);
    text_p95.push_back(
        trials[i].text.timing.p95_ns_per_event);
    text_p99.push_back(
        trials[i].text.timing.p99_ns_per_event);
    text_throughput.push_back(
        trials[i].text.timed_events_per_second);

    prepared_p50.push_back(
        trials[i].prepared.timing.p50_ns_per_event);
    prepared_p95.push_back(
        trials[i].prepared.timing.p95_ns_per_event);
    prepared_p99.push_back(
        trials[i].prepared.timing.p99_ns_per_event);
    prepared_throughput.push_back(
        trials[i].prepared.timed_events_per_second);

    paired_p50_delta.push_back(
        trials[i]
            .text_minus_prepared_p50_ns_per_event);
    paired_average_delta.push_back(
        trials[i]
            .text_minus_prepared_total_avg_ns_per_event);
    order_rate.push_back(
        trials[i].orders_per_event);
  }

  auto sort_all = [](std::vector<double>* values) {
    std::sort(values->begin(), values->end());
  };

  sort_all(&text_p50);
  sort_all(&text_p95);
  sort_all(&text_p99);
  sort_all(&text_throughput);
  sort_all(&prepared_p50);
  sort_all(&prepared_p95);
  sort_all(&prepared_p99);
  sort_all(&prepared_throughput);
  sort_all(&paired_p50_delta);
  sort_all(&paired_average_delta);
  sort_all(&order_rate);

  summary.trial_count = text_p50.size();

  summary.text_p50_median_ns_per_event =
      Percentile(text_p50, 50, 100);
  summary.text_p95_median_ns_per_event =
      Percentile(text_p95, 50, 100);
  summary.text_p99_median_ns_per_event =
      Percentile(text_p99, 50, 100);
  summary.text_events_per_second_median =
      Percentile(text_throughput, 50, 100);

  summary.prepared_p50_median_ns_per_event =
      Percentile(prepared_p50, 50, 100);
  summary.prepared_p95_median_ns_per_event =
      Percentile(prepared_p95, 50, 100);
  summary.prepared_p99_median_ns_per_event =
      Percentile(prepared_p99, 50, 100);
  summary.prepared_events_per_second_median =
      Percentile(prepared_throughput, 50, 100);

  summary.paired_p50_delta_median_ns_per_event =
      Percentile(paired_p50_delta, 50, 100);
  summary.paired_total_avg_delta_median_ns_per_event =
      Percentile(paired_average_delta, 50, 100);
  summary.orders_per_event_median =
      Percentile(order_rate, 50, 100);

  return summary;
}

void PrintCrossTrialSummary(
    const char* name,
    const CrossTrialSummary& summary) {
  std::cout << name << "\n";
  std::cout << "  trial_count="
            << summary.trial_count << "\n";

  std::cout << "  text_p50_median_ns_per_event="
            << summary.text_p50_median_ns_per_event << "\n";
  std::cout << "  text_p95_median_ns_per_event="
            << summary.text_p95_median_ns_per_event << "\n";
  std::cout << "  text_p99_median_ns_per_event="
            << summary.text_p99_median_ns_per_event << "\n";
  std::cout << "  text_events_per_second_median="
            << summary.text_events_per_second_median << "\n";

  std::cout << "  prepared_p50_median_ns_per_event="
            << summary.prepared_p50_median_ns_per_event << "\n";
  std::cout << "  prepared_p95_median_ns_per_event="
            << summary.prepared_p95_median_ns_per_event << "\n";
  std::cout << "  prepared_p99_median_ns_per_event="
            << summary.prepared_p99_median_ns_per_event << "\n";
  std::cout << "  prepared_events_per_second_median="
            << summary.prepared_events_per_second_median << "\n";

  std::cout
      << "  paired_p50_delta_median_ns_per_event="
      << summary.paired_p50_delta_median_ns_per_event
      << "\n";
  std::cout
      << "  paired_total_avg_delta_median_ns_per_event="
      << summary
             .paired_total_avg_delta_median_ns_per_event
      << "\n";
  std::cout << "  orders_per_event_median="
            << summary.orders_per_event_median << "\n";
}

bool WriteCsv(
    const std::string& path,
    const BenchmarkConfig& config,
    const std::vector<PreparedInput>& inputs,
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
      << "text_total_elapsed_ns,"
      << "text_min_ns_per_event,text_p50_ns_per_event,"
      << "text_p95_ns_per_event,text_p99_ns_per_event,"
      << "text_max_ns_per_event,text_events_per_second,"
      << "prepared_total_elapsed_ns,"
      << "prepared_min_ns_per_event,"
      << "prepared_p50_ns_per_event,"
      << "prepared_p95_ns_per_event,"
      << "prepared_p99_ns_per_event,"
      << "prepared_max_ns_per_event,"
      << "prepared_events_per_second,"
      << "text_minus_prepared_p50_ns_per_event,"
      << "text_minus_prepared_total_avg_ns_per_event,"
      << "orders_per_event,"
      << "text_orders_generated,"
      << "text_orders_dropped,"
      << "text_pipeline_rejected,"
      << "text_gateway_rejected,"
      << "prepared_orders_generated,"
      << "prepared_orders_dropped,"
      << "prepared_pipeline_rejected,"
      << "prepared_gateway_rejected,"
      << "text_checksum,prepared_checksum\n";

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
        << trial.text.total_elapsed_ns << ','
        << trial.text.timing.min_ns_per_event << ','
        << trial.text.timing.p50_ns_per_event << ','
        << trial.text.timing.p95_ns_per_event << ','
        << trial.text.timing.p99_ns_per_event << ','
        << trial.text.timing.max_ns_per_event << ','
        << trial.text.timed_events_per_second << ','
        << trial.prepared.total_elapsed_ns << ','
        << trial.prepared.timing.min_ns_per_event << ','
        << trial.prepared.timing.p50_ns_per_event << ','
        << trial.prepared.timing.p95_ns_per_event << ','
        << trial.prepared.timing.p99_ns_per_event << ','
        << trial.prepared.timing.max_ns_per_event << ','
        << trial.prepared.timed_events_per_second << ','
        << trial
               .text_minus_prepared_p50_ns_per_event
        << ','
        << trial
               .text_minus_prepared_total_avg_ns_per_event
        << ','
        << trial.orders_per_event << ','
        << trial.text.counters.orders_generated << ','
        << trial.text.counters.orders_dropped << ','
        << trial.text.counters.pipeline_rejected << ','
        << trial.text.counters.gateway_rejected << ','
        << trial.prepared.counters.orders_generated << ','
        << trial.prepared.counters.orders_dropped << ','
        << trial.prepared.counters.pipeline_rejected << ','
        << trial.prepared.counters.gateway_rejected << ','
        << trial.text.checksum << ','
        << trial.prepared.checksum << '\n';
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

  const std::vector<RawInput> raw_inputs =
      BuildRawInputs(config.workload);

  const std::vector<PreparedInput> inputs =
      PrepareInputs(raw_inputs);

  std::cout << std::fixed << std::setprecision(3);

  std::cout << "Paired path-comparison benchmark smoke run\n";
  std::cout
      << "  warning: local measurement only, not a performance claim\n";
  std::cout
      << "  text_path=parser_to_exchange_simulator\n";
  std::cout
      << "  prepared_path=preparsed_update_to_exchange_simulator\n";
  std::cout
      << "  paths_are_measured_in_the_same_trial\n";
  std::cout
      << "  path_order_alternates_between_trials\n";
  std::cout
      << "  paired_deltas_are_descriptive_not_exact_parser_cost\n";
  std::cout
      << "  file_io_and_feed_replay_are_excluded\n";
  std::cout << "  workload="
            << config.workload << "\n";
  std::cout << "  input_count="
            << inputs.size() << "\n";
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

  const StabilizationResult text_stabilization =
      RunStabilization(
          PathMode::kText,
          config,
          inputs);

  const StabilizationResult prepared_stabilization =
      RunStabilization(
          PathMode::kPrepared,
          config,
          inputs);

  std::cout << "Text-path stabilization\n";
  std::cout << "  elapsed_ns="
            << text_stabilization.elapsed_ns << "\n";
  std::cout << "  orders_generated="
            << text_stabilization.counters.orders_generated
            << "\n";
  std::cout << "  checksum="
            << text_stabilization.checksum << "\n";

  std::cout << "Prepared-path stabilization\n";
  std::cout << "  elapsed_ns="
            << prepared_stabilization.elapsed_ns << "\n";
  std::cout << "  orders_generated="
            << prepared_stabilization.counters.orders_generated
            << "\n";
  std::cout << "  checksum="
            << prepared_stabilization.checksum << "\n";

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

  const CrossTrialSummary all_trials =
      BuildCrossTrialSummary(trials, 0);

  PrintCrossTrialSummary(
      "All-trial descriptive summary",
      all_trials);

  if (trials.size() > 1) {
    const CrossTrialSummary steady_state =
        BuildCrossTrialSummary(trials, 1);

    PrintCrossTrialSummary(
        "Steady-state candidate summary (first trial excluded)",
        steady_state);
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
