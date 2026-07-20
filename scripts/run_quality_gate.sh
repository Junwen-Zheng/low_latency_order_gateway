#!/usr/bin/env bash
set -euo pipefail

strict_build_dir="${1:-build-quality}"
sanitizer_build_dir="${2:-build-sanitizers}"

rm -rf "$strict_build_dir"

cmake \
  -S . \
  -B "$strict_build_dir" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLLGW_WARNINGS_AS_ERRORS=ON

cmake --build "$strict_build_dir"

ctest \
  --test-dir "$strict_build_dir" \
  --output-on-failure

./scripts/run_sanitizers.sh "$sanitizer_build_dir"

echo "Production-readiness quality gate passed."
