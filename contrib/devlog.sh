#!/usr/bin/env bash
# devlog.sh - development logging driver for the lincity-ng fork.
#
# Every run goes into build/logs/<UTC timestamp>_<tag>/, so the last
# execution is identifiable by date. `latest/` is a symlink to the most
# recent run. run.json records the exact command + env for reproduction.
#
# Subcommands:
#   devlog.sh unit  [--filter SUBSTR] [--ts] [--asan] [--gdb] [--tag TAG]
#   devlog.sh sim   [--days N] [--seed N] [--scenario S] [--log-areas A]
#                   [--log-level L] [--asan] [--gdb] [--tag TAG]
#   devlog.sh save  [--log-areas A] [--log-level L] [--asan] [--gdb] [--tag TAG]
#   devlog.sh ctest [--asan] [--tag TAG]     (all 3 test layers)
#   devlog.sh game  [args...] [--asan] [--gdb] [--tag TAG]  (run the game binary)
#   devlog.sh list                           (show recent runs, newest last)
#   devlog.sh latest                          (show latest/ dir and run.json)
#   devlog.sh bt TAG|PATH [--asan]            (open saved gdb backtrace file)
#   devlog.sh clean [KEEP]                    (prune runs, keep newest KEEP)
#
# Env:
#   LINCITYNG_DEVLOG_KEEP   retention: number of runs to keep (default 30)
#   LINCITYNG_LOG_LEVEL     default level for the run (default trace)
#   LINCITYNG_LOG_AREAS     area overrides (e.g. "sim=debug,econ=info")
#   LINCITYNG_DEVLOG_DIR    log root (default build/logs)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${LINCITYNG_DEVLOG_BUILD:-$REPO_ROOT/build}"
ASAN_DIR="${LINCITYNG_DEVLOG_ASAN:-$REPO_ROOT/build-asan}"
LOGS_ROOT="${LINCITYNG_DEVLOG_DIR:-$BUILD_DIR/logs}"
KEEP="${LINCITYNG_DEVLOG_KEEP:-30}"
DEFAULT_LEVEL="${LINCITYNG_LOG_LEVEL:-trace}"
AREA_FLAG=()
LEVEL_FLAG=()

if [[ -n "${LINCITYNG_LOG_AREAS:-}" ]]; then
  AREA_FLAG=(--log-areas "$LINCITYNG_LOG_AREAS")
fi

tag() {
  local run_tag="${1:-dev}"
  echo "$(date -u +%Y%m%d-%H%M%S)_${run_tag}"
}

# latest symlink + retention pruning, called after a run dir is created.
finalize_run() {
  local run_dir="$1"
  ln -sfn "$(basename "$run_dir")" "$LOGS_ROOT/latest"
  mapfile -t old < <(ls -1dt "$LOGS_ROOT"/[0-9]*_* 2>/dev/null || true)
  local n_old=${#old[@]}
  if (( n_old > KEEP )); then
    for (( i = KEEP; i < n_old; i++ )); do
      rm -rf "${old[$i]}"
    done
  fi
}

write_run_json() {
  local run_dir="$1" name="$2"
  shift 2
  local cmdline=()
  for arg in "$@"; do
    cmdline+=("$arg")
  done
  {
    printf '{\n'
    printf '  "cmd": ['
    local first=1
    for arg in "${cmdline[@]}"; do
      if (( first )); then first=0; else printf ', '; fi
      printf '"%s"' "$(printf '%s' "$arg" | sed 's/\\/\\\\/g; s/"/\\"/g')"
    done
    printf '],\n'
    printf '  "tag": "%s",\n' "$(basename "$run_dir")"
    printf '  "date_utc": "%s",\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
    printf '  "retention_keep": %s\n' "$KEEP"
    printf '}\n'
  } > "$run_dir/run.json"
}

# Parse common flags: --asan --gdb --tag --log-areas --log-level
# Leaves positional args in $ARGS. Sets $USE_ASAN, $USE_GDB, $RUN_TAG.
# $1 = default tag, rest = CLI args.
parse_common() {
  USE_ASAN=0
  USE_GDB=0
  RUN_TAG="$1"
  ARGS=()
  shift
  while (( $# )); do
    case "$1" in
      --asan) USE_ASAN=1; shift;;
      --gdb)  USE_GDB=1; shift;;
      --tag)  RUN_TAG="$2"; shift 2;;
      --log-areas) AREA_FLAG=(--log-areas "$2"); shift 2;;
      --log-level) LEVEL_FLAG=(--log-level "$2"); shift 2;;
      *) ARGS+=("$1"); shift;;
    esac
  done
}

run_binary() {
  local name="$1" bin="$2"
  shift 2
  local run_dir="$LOGS_ROOT/$(tag "$name")"
  mkdir -p "$run_dir"
  write_run_json "$run_dir" "$name" "$bin" "$@"
  local stdout="$run_dir/stdout.log"
  local stderr="$run_dir/stderr.log"

  export LINCITYNG_LOG_LEVEL="${LEVEL_FLAG[1]:-$DEFAULT_LEVEL}"
  if (( ${#AREA_FLAG[@]} )); then
    export LINCITYNG_LOG_AREAS="${AREA_FLAG[1]}"
  fi
  export LINCITYNG_LOG_FILE="$run_dir/lincity-ng.log"

  echo "== $name -> $run_dir" | tee "$stdout"
  local rc=0
  if (( USE_GDB )); then
    # gdb batch: stdout holds program + backtrace, stderr gets gdb's own output
    gdb -batch -ex run -ex bt --args "$bin" "$@" >"$stdout" 2>"$stderr"
    rc=$?
    if grep -q '^Thread\|#0 ' "$stdout"; then
      cp "$stdout" "$run_dir/bt.txt"
    fi
  else
    "$bin" "$@" >"$stdout" 2>"$stderr"
    rc=$?
  fi
  echo "exit=$rc" >> "$stdout"
  finalize_run "$run_dir"
  if (( rc != 0 )); then
    echo "FAILED (exit $rc); logs in $run_dir" >&2
  fi
  return $rc
}

cmd_unit() {
  parse_common unit "$@"
  local bin_dir="$BUILD_DIR/tests/unit"
  [[ $USE_ASAN == 1 ]] && bin_dir="$ASAN_DIR/tests/unit"
  local bin="$bin_dir/lincity-test-unit"
  [[ -x "$bin" ]] || { echo "missing: $bin" >&2; return 1; }
  run_binary unit "$bin" "${ARGS[@]}"
}

cmd_sim() {
  parse_common sim "$@"
  local bin_dir="$BUILD_DIR/tests/sim"
  [[ $USE_ASAN == 1 ]] && bin_dir="$ASAN_DIR/tests/sim"
  local bin="$bin_dir/lincity-test-sim"
  [[ -x "$bin" ]] || { echo "missing: $bin" >&2; return 1; }
  local has_days=0
  for a in "${ARGS[@]}"; do [[ "$a" == --days ]] && has_days=1; done
  if (( has_days )); then
    run_binary sim "$bin" "${ARGS[@]}"
  else
    run_binary sim "$bin" --days 1000 "${ARGS[@]}"
  fi
}

cmd_save() {
  parse_common save "$@"
  local bin_dir="$BUILD_DIR/tests/save"
  [[ $USE_ASAN == 1 ]] && bin_dir="$ASAN_DIR/tests/save"
  local bin="$bin_dir/lincity-test-save"
  [[ -x "$bin" ]] || { echo "missing: $bin" >&2; return 1; }
  run_binary save "$bin" "${ARGS[@]}"
}

cmd_ctest() {
  parse_common ctest "$@"
  local dir="$BUILD_DIR"
  [[ $USE_ASAN == 1 ]] && dir="$ASAN_DIR"
  local run_dir="$LOGS_ROOT/$(tag ctest)"
  mkdir -p "$run_dir"
  write_run_json "$run_dir" ctest "ctest --test-dir $dir"
  export LINCITYNG_LOG_LEVEL="${LEVEL_FLAG[1]:-$DEFAULT_LEVEL}"
  if (( ${#AREA_FLAG[@]} )); then
    export LINCITYNG_LOG_AREAS="${AREA_FLAG[1]}"
  fi
  # unit test sinks only via env; sim/save forward their own --log-* flags
  ctest --test-dir "$dir" "${ARGS[@]}" >"$run_dir/stdout.log" 2>"$run_dir/stderr.log" \
    || true
  local rc=$?
  echo "exit=$rc" >> "$run_dir/stdout.log"
  finalize_run "$run_dir"
  return $rc
}

cmd_game() {
  parse_common game "$@"
  local dir="$BUILD_DIR"
  [[ $USE_ASAN == 1 ]] && dir="$ASAN_DIR"
  local bin="$dir/bin/lincity-ng"
  [[ -x "$bin" ]] || { echo "missing: $bin (build the game target first)" >&2; return 1; }
  # forward SDL/other env from the caller; headless drivers are opt-in via
  # SDL_VIDEODRIVER, so a real window opens unless the caller sets it.
  run_binary game "$bin" "${ARGS[@]}"
}

cmd_list() {
  mapfile -t runs < <(ls -1dt "$LOGS_ROOT"/[0-9]*_* 2>/dev/null || true)
  if (( ${#runs[@]} == 0 )); then
    echo "no runs in $LOGS_ROOT"
    return 0
  fi
  local n=${#runs[@]}
  local i=$((n - 1))
  for (( ; i >= 0; i-- )); do
    local r="${runs[$i]}"
    local rc="$(sed -n 's/^exit=//p' "$r/stdout.log" 2>/dev/null | tail -1)"
    printf '%s  exit=%s  %s\n' "$(basename "$r")" "${rc:-?}" "$r"
  done
}

cmd_latest() {
  local latest="$LOGS_ROOT/latest"
  [[ -L "$latest" ]] || { echo "no latest run yet" >&2; return 1; }
  echo "$latest -> $(readlink "$latest")"
  cat "$latest/run.json" 2>/dev/null || true
}

cmd_bt() {
  local target="${1:-latest}"
  local run_dir
  if [[ "$target" == /* ]] || [[ "$target" == ./* ]]; then
    run_dir="$target"
  elif [[ -d "$LOGS_ROOT/$target" ]]; then
    run_dir="$LOGS_ROOT/$target"
  else
    run_dir="$LOGS_ROOT/latest"
  fi
  local bt_file="$run_dir/bt.txt"
  [[ -f "$bt_file" ]] || { echo "no $bt_file (rerun with --gdb)" >&2; return 1; }
  cat "$bt_file"
}

cmd_clean() {
  local keep="${1:-$KEEP}"
  mapfile -t runs < <(ls -1dt "$LOGS_ROOT"/[0-9]*_* 2>/dev/null || true)
  local n=${#runs[@]}
  local removed=0
  for (( i = keep; i < n; i++ )); do
    rm -rf "${runs[$i]}"
    removed=$((removed + 1))
  done
  echo "removed $removed run(s); kept ${#runs[@]} -> ${keep}"
  local latest="$LOGS_ROOT/latest"
  if [[ -L "$latest" ]] && [[ ! -e "$latest" ]]; then
    rm -f "$latest"
  fi
}

main() {
  local cmd="${1:-help}"
  shift || true
  case "$cmd" in
    unit)   cmd_unit "$@";;
    sim)    cmd_sim "$@";;
    save)   cmd_save "$@";;
    ctest)  cmd_ctest "$@";;
    game)   cmd_game "$@";;
    list)   cmd_list "$@";;
    latest) cmd_latest "$@";;
    bt)     cmd_bt "$@";;
    clean)  cmd_clean "${1:-$KEEP}";;
    help|*) echo "usage: $0 {unit|sim|save|ctest|game|list|latest|bt|clean} [flags]";;
  esac
}

main "$@"