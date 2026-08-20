#!/usr/bin/env bash
set -euo pipefail

repo_root="${1:?repository root argument is required}"
tmp_root="$(mktemp -d)"
trap 'rm -rf "$tmp_root"' EXIT

mkdir -p "$tmp_root/bin"
cp "$repo_root/tests/fixtures/fake_ctest.sh" "$tmp_root/bin/ctest"
chmod +x "$tmp_root/bin/ctest"

run_driver() {
  local logs_root="$1" rc_expected="$2" sleep_for="$3" run_tag="$4"
  local rc=0
  set +e
  PATH="$tmp_root/bin:$PATH" \
    LINCITYNG_DEVLOG_DIR="$logs_root" \
    LINCITYNG_DEVLOG_BUILD="$tmp_root/build" \
    FAKE_CTEST_RC="$rc_expected" \
    FAKE_CTEST_SLEEP="$sleep_for" \
    "$repo_root/contrib/devlog.sh" ctest --tag "$run_tag"
  rc=$?
  set -e
  [[ "$rc" -eq "$rc_expected" ]]
}

failure_logs="$tmp_root/failure-logs"
run_driver "$failure_logs" 7 0 failure
[[ -L "$failure_logs/latest" ]]
grep -q '^exit=7$' "$failure_logs/latest/stdout.log"

parallel_logs="$tmp_root/parallel-logs"
run_driver "$parallel_logs" 0 0.2 concurrent-a &
pid_a=$!
run_driver "$parallel_logs" 0 0.2 concurrent-b &
pid_b=$!
wait "$pid_a"
wait "$pid_b"

[[ -L "$parallel_logs/latest" ]]
[[ -e "$parallel_logs/latest/run.json" ]]
mapfile -t parallel_runs < <(find "$parallel_logs" -mindepth 1 -maxdepth 1 -type d -name '*_concurrent-*')
[[ "${#parallel_runs[@]}" -eq 2 ]]
