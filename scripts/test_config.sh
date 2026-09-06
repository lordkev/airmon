#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CJSON="${IDF_PATH:-$ROOT/.tools/esp-idf}/components/json/cJSON"
mkdir -p "$ROOT/tmp/tests"
cc -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations -fsanitize=address,undefined -g \
  -I "$CJSON" -c "$CJSON/cJSON.c" -o "$ROOT/tmp/tests/cjson.o"
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
  -I "$ROOT/tests/fakes" -I "$ROOT/firmware/main" \
  -I "$ROOT/firmware/components/airmon_core/include" -I "$CJSON" \
  "$ROOT/tests/test_config.c" "$ROOT/firmware/main/config.c" "$ROOT/tmp/tests/cjson.o" \
  -lm -o "$ROOT/tmp/tests/config"
"$ROOT/tmp/tests/config"
