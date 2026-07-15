# Continuous Integration

## Scope

The GitHub Actions workflow is defined in:

`.github/workflows/ci.yml`

It runs on:

- pushes to `main`
- pull requests
- manual workflow dispatch

## CI Job

The job performs:

1. repository checkout
2. installation of CMake and the C++ compiler
3. all repository correctness checks
4. the quick seven-case benchmark smoke suite
5. validation that seven non-empty benchmark CSV files were created

The workflow uses a read-only repositor permission and a twenty-minute timeout.

## Local Equivalent

Run:

`./scripts/run_ci_smoke.sh benchmark_results/local_ci_smoke`

This runs the same repository checks and quick benchmark smoke suite used by CI.

## Performance Policy

CI validates buildability, correctness, benchmark execution, invariant checks, and CSV creation.

CI does not compare latency or throughput values against thresholds.

Developer-machine and GitHub-hosted-runner measurements are not directly comparable.

## Benchmark Artifacts

Generated CSV files remain ignored local artifacts.

The workflow does not publish benchmark values as releases, badges, or performance claims.
