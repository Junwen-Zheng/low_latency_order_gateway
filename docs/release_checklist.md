# v1.0.0 Release Checklist

## Code and Repository

- [ ] working tree is clean before starting
- [ ] branch is `main`
- [ ] branch is up to date with `origin/main`
- [ ] no unintended generated files are tracked
- [ ] `git diff --check` passes

## Validation

- [ ] `./scripts/run_final_validation.sh` passes
- [ ] strict Release build passes with warnings as errors
- [ ] 20/20 strict CTest cases pass
- [ ] sanitizer build passes
- [ ] 20/20 sanitizer CTest cases pass
- [ ] seven benchmark smoke CSV files are non-empty
- [ ] benchmark thresholds remain disabled

## Documentation

- [ ] README reflects final capabilities
- [ ] architecture reflects risk, lifecycle, actions, and reports
- [ ] project status describes the v1 scope
- [ ] final report labels benchmarks as local measurements
- [ ] production limitations are explicit
- [ ] portfolio summary avoids unsupported claims
- [ ] study log includes Day 24

## Commit and Push

```bash
git add \
  README.md \
  docs/architecture.md \
  docs/project_status.md \
  docs/final_report.md \
  docs/portfolio_summary.md \
  docs/release_checklist.md \
  docs/study_log.md \
  scripts/run_final_validation.sh

git commit -m "Finalize v1 portfolio release"

GIT_SSH_COMMAND='ssh -o IdentitiesOnly=yes -i ~/.ssh/id_ed25519' \
  git push origin main
```

## Tag

Only after the final commit is pushed and the working tree is clean:

```bash
git tag -a v1.0.0 -m "Low-latency order gateway simulator v1.0.0"

GIT_SSH_COMMAND='ssh -o IdentitiesOnly=yes -i ~/.ssh/id_ed25519' \
  git push origin v1.0.0
```

## Final Verification

```bash
git status
git log -1 --oneline
git show --stat --oneline v1.0.0
git tag --list
```

Expected state:

```text
main is up to date with origin/main
working tree clean
v1.0.0 resolves to the final release commit
```
