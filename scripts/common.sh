#!/bin/bash
# Shared config for build.sh / upload.sh / monitor.sh. Not meant to be run
# directly -- sourced by the other scripts.

# Plain ESP-WROOM-32 dev board (original ESP32, not S3/C3/etc). Verify
# against your installed esp32:esp32 core version -- tested against 2.0.17.
FQBN="esp32:esp32:esp32"

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
