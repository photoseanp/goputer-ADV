#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
CLI="${ARDUINO_CLI:-arduino-cli}"
FQBN='m5stack:esp32:m5stack_cardputer:FlashSize=8M,PartitionScheme=default_8MB,PSRAM=disabled'
"$CLI" compile --jobs 2 --fqbn "$FQBN" --warnings all --output-dir "$PWD/build" "$PWD/goputer_ADV"
mkdir -p dist
cp build/goputer_ADV.ino.bin dist/goputer_ADV.bin
printf '\nApp-only binary: %s/dist/goputer_ADV.bin\n' "$PWD"
