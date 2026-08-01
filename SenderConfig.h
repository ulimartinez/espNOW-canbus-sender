#pragma once

// Central mode selection for this firmware. Change behavior here only —
// no other file should need edits to switch modes.
//
// WIRELESS_BENCH_TEST_MODE: when 1, TWAI is never initialized (no CAN GPIOs
// touched, no transceiver assumed present). A VehicleSimulator generates
// VehicleData instead of a decoded CAN frame, but it still flows through
// the exact same GaugePacketBuilder + EspNowSender used in production. This
// validates the wireless link, packet protocol, and display behavior before
// the vehicle is ever involved. See docs/validation-plan.md.
//
// Set to 0 for real vehicle-CAN operation. Overridable at build time (e.g.
// `--build-property build.extra_flags=-DWIRELESS_BENCH_TEST_MODE=0`) so
// both code paths can be compile-checked without editing this file.
#ifndef WIRELESS_BENCH_TEST_MODE
#define WIRELESS_BENCH_TEST_MODE 1
#endif

// Master enables. Independent of WIRELESS_BENCH_TEST_MODE so bench
// configurations like "CAN validation only, no ESP-NOW" are still possible.
#define CAN_ENABLED (!WIRELESS_BENCH_TEST_MODE)
#define ESPNOW_ENABLED 1

// Raw CAN frame logging (candump-style). Leave off in normal operation —
// printing every frame at bus speed can starve the receive queue.
#define CAN_RAW_LOG_ENABLED 0

// Extra diagnostic prints beyond the periodic [STATUS] line.
#define VERBOSE_LOGGING_ENABLED 0

#if WIRELESS_BENCH_TEST_MODE
#warning "Building firmware in WIRELESS BENCH TEST MODE -- simulated data will be transmitted, not real vehicle data"
#endif

// A production build must never silently transmit simulated oil-pressure
// data. Flip PRODUCTION_BUILD to 1 (e.g. via a build flag) to make this a
// hard compile-time error if WIRELESS_BENCH_TEST_MODE was left enabled.
#ifndef PRODUCTION_BUILD
#define PRODUCTION_BUILD 0
#endif

static_assert(!PRODUCTION_BUILD || !WIRELESS_BENCH_TEST_MODE,
              "Production builds cannot enable WIRELESS_BENCH_TEST_MODE");

constexpr char FIRMWARE_NAME[] = "gr86-can-espnow-sender";
constexpr char FIRMWARE_VERSION[] = "0.1.0";

#ifndef FIRMWARE_BUILD_GIT_HASH
#define FIRMWARE_BUILD_GIT_HASH "unknown"
#endif

constexpr uint32_t ESPNOW_SEND_INTERVAL_MS = 40;   // 25 Hz, within 20-50 Hz range
constexpr uint32_t SERIAL_STATUS_INTERVAL_MS = 1000;
constexpr uint32_t TEST_LOG_INTERVAL_MS = 1000;
