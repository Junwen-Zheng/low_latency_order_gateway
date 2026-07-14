#!/usr/bin/env bash
set -euo pipefail

mode="quick"
prefix="benchmark_results/day17_suite"

usage() {
  echo "Usage: $0 [--mode quick|full] [--prefix PATH_PREFIX]"
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --mode)
      [[ $# -ge 2 ]] || { echo "Missing value for --mode" >&2; exit 1; }
      mode="$2"
      shift 2
      ;;
    --prefix)
      [[ $# -ge 2 ]] || { echo "Missing value for --prefix" >&2; exit 1; }
      prefix="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

case "$mode" in
  quick)
    stabilization_iterations=10000
    warmup_iterations=640
    measured_iterations=6400
    trials=3
    batch_size=64
    ;;
  full)
    stabilization_iterations=100000
    warmup_iterations=6400
    measured_iterations=64000
    trials=5
    batch_size=64
    ;;
  *)
    echo "Invalid mode: $mode. Expected quick or full." >&2
    exit 1
    ;;
esac

mkdir -p "$(dirname "$prefix")"
log_dir="${TMPDIR:-/tmp}/llgw_benchmark_suite"
mkdir -p "$log_dir"

echo "Benchmark suite"
echo "  mode=$mode"
echo "  output_prefix=$prefix"
echo "  progress_output=concise"
echo "  detailed_logs=$log_dir"
echo "  warning=local_measurement_only_not_a_performance_claim"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

run_case() {
  local number="$1"
  local label="$2"
  local executable="$3"
  local csv_path="$4"
  shift 4

  local log_path="$log_dir/case_${number}.log"

  echo "[$number/7] $label"
  "$executable" "$@" --csv "$csv_path" > "$log_path"

  [[ -s "$csv_path" ]] || {
    echo "Missing or empty CSV: $csv_path" >&2
    exit 1
  }

  [[ -s "$log_path" ]] || {
    echo "Missing or empty log: $log_path" >&2
    exit 1
  }

  echo "      completed: $csv_path"
}

common_args=(
  --stabilization-iterations "$stabilization_iterations"
  --warmup "$warmup_iterations"
  --iterations "$measured_iterations"
  --trials "$trials"
  --batch-size "$batch_size"
)

run_case 1 "Parser: single input" \
  ./build/bench_parser \
  "${prefix}_parser_single.csv" \
  "${common_args[@]}" \
  --input-set single

run_case 2 "Parser: varied input" \
  ./build/bench_parser \
  "${prefix}_parser_varied.csv" \
  "${common_args[@]}" \
  --input-set varied

run_case 3 "End-to-end: mixed" \
  ./build/bench_end_to_end \
  "${prefix}_end_to_end_mixed.csv" \
  "${common_args[@]}" \
  --workload mixed

run_case 4 "End-to-end: all orders" \
  ./build/bench_end_to_end \
  "${prefix}_end_to_end_all_orders.csv" \
  "${common_args[@]}" \
  --workload all-orders

run_case 5 "Path comparison: no orders" \
  ./build/bench_path_comparison \
  "${prefix}_path_no_orders.csv" \
  "${common_args[@]}" \
  --workload no-orders

run_case 6 "Path comparison: mixed" \
  ./build/bench_path_comparison \
  "${prefix}_path_mixed.csv" \
  "${common_args[@]}" \
  --workload mixed

run_case 7 "Path comparison: all orders" \
  ./build/bench_path_comparison \
  "${prefix}_path_all_orders.csv" \
  "${common_args[@]}" \
  --workload all-orders

echo
echo "Benchmark suite completed successfully."
echo "Detailed logs:"
for log in "$log_dir"/case_*.log; do
  echo "  $log"
done
