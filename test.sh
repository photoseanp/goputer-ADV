#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT
CXX="${CXX:-g++}"
FLAGS=(-std=c++11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer)
CORE=(goputer_ADV/GoEngine.cpp goputer_ADV/Bot.cpp goputer_ADV/SaveCodec.cpp)
"$CXX" "${FLAGS[@]}" tests/core_tests.cpp "${CORE[@]}" -o "$TMP/core"
"$TMP/core"
"$CXX" "${FLAGS[@]}" -Itests/stubs tests/ui_tests.cpp "${CORE[@]}" goputer_ADV/Storage.cpp -o "$TMP/ui"
"$TMP/ui"
