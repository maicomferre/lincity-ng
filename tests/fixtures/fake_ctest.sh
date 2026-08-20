#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${FAKE_CTEST_SLEEP:-}" ]]; then
  sleep "$FAKE_CTEST_SLEEP"
fi
exit "${FAKE_CTEST_RC:-0}"
