#!/usr/bin/env bash
set -euo pipefail
CLI="${ARDUINO_CLI:-arduino-cli}"
URL='https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json'
"$CLI" core update-index --additional-urls "$URL"
"$CLI" core install m5stack:esp32@3.2.2 --additional-urls "$URL"
"$CLI" lib update-index
"$CLI" lib install 'M5GFX@0.3.0'
"$CLI" lib install 'M5Unified@0.3.0' --no-deps
"$CLI" lib install 'IRremote@4.7.1' 'LibSSH-ESP32@5.10.0'
"$CLI" lib install 'M5Cardputer@1.1.1' --no-deps
