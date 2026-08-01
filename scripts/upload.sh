#!/bin/bash
# Compiles and flashes the firmware.
# Usage: ./scripts/upload.sh /dev/cu.usbserial-XXXX [--vehicle-can]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common.sh"

if [ $# -lt 1 ]; then
    echo "usage: $0 <serial-port> [--vehicle-can]" >&2
    echo "  e.g. $0 /dev/cu.usbserial-0001" >&2
    echo "  list candidate ports with: arduino-cli board list" >&2
    exit 1
fi
PORT="$1"

EXTRA_FLAGS=()
if [ "${2:-}" = "--vehicle-can" ]; then
    EXTRA_FLAGS=(--build-property "build.extra_flags=-DWIRELESS_BENCH_TEST_MODE=0")
    echo "Overriding SenderConfig.h: forcing WIRELESS_BENCH_TEST_MODE=0 (real vehicle CAN)"
fi

if ! command -v arduino-cli >/dev/null 2>&1; then
    echo "error: arduino-cli not found on PATH." >&2
    exit 1
fi

echo "Building $SKETCH_DIR"
echo "FQBN: $FQBN"
arduino-cli compile --fqbn "$FQBN" ${EXTRA_FLAGS[@]+"${EXTRA_FLAGS[@]}"} "$SKETCH_DIR"

echo "Uploading to $PORT"
arduino-cli upload --fqbn "$FQBN" -p "$PORT" "$SKETCH_DIR"
