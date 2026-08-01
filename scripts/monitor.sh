#!/bin/bash
# Opens a serial monitor at the firmware's baud rate (115200).
# Usage: ./scripts/monitor.sh /dev/cu.usbserial-XXXX
set -euo pipefail

if [ $# -lt 1 ]; then
    echo "usage: $0 <serial-port>" >&2
    echo "  e.g. $0 /dev/cu.usbserial-0001" >&2
    echo "  list candidate ports with: arduino-cli board list" >&2
    exit 1
fi
PORT="$1"

if ! command -v arduino-cli >/dev/null 2>&1; then
    echo "error: arduino-cli not found on PATH." >&2
    exit 1
fi

arduino-cli monitor -p "$PORT" -c baudrate=115200
