#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$ROOT/tmp/tests"
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I "$ROOT/firmware/components/airmon_core/include" \
  "$ROOT/tests/test_core.c" "$ROOT/firmware/components/airmon_core/airmon_core.c" -lm -o "$ROOT/tmp/tests/core"
"$ROOT/tmp/tests/core"
