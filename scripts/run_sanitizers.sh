#!/usr/bin/env bash
set -euo pipefail

build_dir="${1:-build-sanitizers}"

rm -rf "$build_dir"

cmake \
  -S . \
  -B "$build_dir" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DLLGW_WARNINGS_AS_ERRORS=ON \
  -DLLGW_ENABLE_SANITIZERS=ON

cmake --build "$build_dir"

if [[ "$(uname -s)" == "Darwin" ]]; then
  echo "Running UndefinedBehaviorSanitizer suite on macOS."
  UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}" \
  ctest \
    --test-dir "$build_dir" \
    --output-on-failure \
    --timeout 30
else
  echo "Running AddressSanitizer and UndefinedBehaviorSanitizer suite."
  ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:detect_leaks=0}" \
  UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=1:print_stacktrace=1}" \
  ctest \
    --test-dir "$build_dir" \
    --output-on-failure \
    --timeout 30
fi

echo "Sanitizer checks passed."
