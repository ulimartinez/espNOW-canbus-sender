#!/bin/bash
# Convenience wrapper: opens the serial monitor and tees raw `[CAN] ...`
# log lines to a timestamped file for later analysis. Enable raw logging
# first over Serial (`can start`) or build with CAN_RAW_LOG_ENABLED=1.
# Usage: ./scripts/capture_can.sh /dev/cu.usbserial-XXXX [output.log]
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $0 <serial-port> [output.log]" >&2
    exit 1
fi
PORT="$1"
OUT="${2:-can_capture_$(date +%Y%m%d_%H%M%S).log}"

if ! command -v arduino-cli >/dev/null 2>&1; then
    echo "error: arduino-cli not found on PATH." >&2
    exit 1
fi

echo "Logging to $OUT (Ctrl-C to stop)"
arduino-cli monitor -p "$PORT" -c baudrate=115200 | tee "$OUT"
