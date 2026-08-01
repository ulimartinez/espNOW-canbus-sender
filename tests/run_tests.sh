#!/bin/bash
# Builds and runs all host-side (no Arduino/ESP-IDF dependency) unit tests
# with the system g++ -- no Arduino toolchain needed. Covers decoder
# formulas, CRC/checksum, packet building, and warning-threshold logic.
# Usage: ./tests/run_tests.sh
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

if ! command -v g++ >/dev/null 2>&1; then
    echo "error: g++ not found on PATH." >&2
    exit 1
fi

# Every test file is linked against this full set of host-safe (Arduino-free)
# sources; unused symbols per test are harmless.
HOST_SOURCES=(
    "$ROOT_DIR/SignalDecoders.cpp"
    "$ROOT_DIR/VehicleCanDecoder.cpp"
    "$ROOT_DIR/GaugePacket.cpp"
    "$ROOT_DIR/PressureWarningLogic.cpp"
    "$ROOT_DIR/GaugePacketBuilder.cpp"
)

BUILD_DIR="$(mktemp -d)"
trap 'rm -rf "$BUILD_DIR"' EXIT

FAILED=0
for test_file in "$SCRIPT_DIR"/*/test_*.cpp; do
    name="$(basename "$test_file" .cpp)"
    bin="$BUILD_DIR/$name"
    echo "== $name =="
    if ! g++ -std=c++17 -Wall -I"$ROOT_DIR" -o "$bin" "$test_file" "${HOST_SOURCES[@]}"; then
        echo "BUILD FAILED: $name"
        FAILED=1
        continue
    fi
    if ! "$bin"; then
        FAILED=1
    fi
    echo
done

if [ "$FAILED" -ne 0 ]; then
    echo "One or more test suites failed."
    exit 1
fi
echo "All decoder/protocol/checksum/warning tests passed."
