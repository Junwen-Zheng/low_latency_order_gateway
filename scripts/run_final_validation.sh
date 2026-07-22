#!/usr/bin/env bash
set -euo pipefail

benchmark_prefix="${1:-benchmark_results/final_validation}"

echo "Final v1 validation"
echo "  benchmark_prefix=$benchmark_prefix"
echo "  performance_thresholds=disabled"

required_files=(
  README.md
  docs/architecture.md
  docs/project_status.md
  docs/production_readiness.md
  docs/final_report.md
  docs/portfolio_summary.md
  docs/release_checklist.md
  docs/study_log.md
  scripts/run_quality_gate.sh
  scripts/run_benchmark_suite.sh
)

for file in "${required_files[@]}"; do
  if [[ ! -s "$file" ]]; then
    echo "Missing or empty required file: $file" >&2
    exit 1
  fi
done

git diff --check

stale_patterns=(
  "No latency benchmarks yet"
  "No benchmark harness yet"
  "No owned symbol storage for queued orders yet"
  "queued order symbols currently require careful lifetime handling"
)

for pattern in "${stale_patterns[@]}"; do
  if grep -RInF \
      --exclude-dir=.git \
      --exclude-dir=build \
      --exclude-dir=build-quality \
      --exclude-dir=build-sanitizers \
      "$pattern" \
      README.md docs/architecture.md docs/project_status.md; then
    echo "Stale release documentation found: $pattern" >&2
    exit 1
  fi
done

./scripts/run_quality_gate.sh

./scripts/run_benchmark_suite.sh \
  --mode quick \
  --prefix "$benchmark_prefix"

csv_count="$(
  find "$(dirname "$benchmark_prefix")" \
    -maxdepth 1 \
    -type f \
    -name "$(basename "$benchmark_prefix")_*.csv" \
    -size +0c |
  wc -l |
  tr -d ' '
)"

if [[ "$csv_count" -ne 7 ]]; then
  echo "Expected 7 non-empty benchmark CSV files; found $csv_count." >&2
  exit 1
fi

echo "Non-empty benchmark CSV files: $csv_count"
echo "Final validation passed."
