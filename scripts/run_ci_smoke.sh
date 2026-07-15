#!/usr/bin/env bash
set -euo pipefail

prefix="${1:-benchmark_results/ci_smoke}"

echo "CI smoke checks"
echo "  benchmark_prefix=$prefix"
echo "  performance_thresholds=disabled"

./scripts/run_tests.sh

./scripts/run_benchmark_suite.sh \
  --mode quick \
  --prefix "$prefix"

csv_count="$(
  find "$(dirname "$prefix")" \
    -maxdepth 1 \
    -type f \
    -name "$(basename "$prefix")_*.csv" \
    -size +0c |
  wc -l |
  tr -d ' '
)"

echo "Non-empty benchmark CSV files: $csv_count"

if [[ "$csv_count" -ne 7 ]]; then
  echo "Expected 7 non-empty benchmark CSV files." >&2
  exit 1
fi

echo "CI smoke checks passed."
