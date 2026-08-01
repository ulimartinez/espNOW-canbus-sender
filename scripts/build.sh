#!/bin/bash
# Compiles the firmware with arduino-cli.
# Requires: arduino-cli installed, the esp32 board package installed
# (arduino-cli core install esp32:esp32). No third-party libraries are
# required -- only the ESP32 Arduino core (WiFi, esp_now, driver/twai).
#
# Usage:
#   ./scripts/build.sh                              # WIRELESS_BENCH_TEST_MODE (default, see SenderConfig.h)
#   ./scripts/build.sh --vehicle-can                 # force real CAN mode without editing SenderConfig.h
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

if ! command -v arduino-cli >/dev/null 2>&1; then
    echo "error: arduino-cli not found on PATH. Install it: https://arduino.github.io/arduino-cli/latest/installation/" >&2
    exit 1
fi

EXTRA_FLAGS=()
if [ "${1:-}" = "--vehicle-can" ]; then
    EXTRA_FLAGS=(--build-property "build.extra_flags=-DWIRELESS_BENCH_TEST_MODE=0")
    echo "Overriding SenderConfig.h: forcing WIRELESS_BENCH_TEST_MODE=0 (real vehicle CAN)"
fi

echo "Building $SKETCH_DIR"
echo "FQBN: $FQBN"
arduino-cli compile --fqbn "$FQBN" ${EXTRA_FLAGS[@]+"${EXTRA_FLAGS[@]}"} "$SKETCH_DIR"
