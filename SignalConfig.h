#pragma once

#include <stdint.h>

// Per-signal plausibility limits and freshness timeouts. Configurable in
// one place so bench-testing can loosen limits without touching decoder
// logic. See docs/can/signal-map.md for the confidence level of each
// mapping (manufacturer-confirmed vs. supplied-profile-only).

// --- Oil pressure (0x171, manufacturer-confirmed) ---
constexpr float MIN_VALID_OIL_PRESSURE_PSI = 0.0f;
constexpr float MAX_VALID_OIL_PRESSURE_PSI = 160.0f;

// --- Engine RPM (0x040) ---
constexpr uint16_t MIN_VALID_RPM = 0;
constexpr uint16_t MAX_VALID_RPM = 9000;

// --- Accelerator position (0x040) ---
constexpr float MIN_VALID_ACCELERATOR_PERCENT = 0.0f;
constexpr float MAX_VALID_ACCELERATOR_PERCENT = 100.0f;

// --- Oil / coolant temperature (0x345) ---
constexpr float MIN_VALID_TEMPERATURE_C = -40.0f;
constexpr float MAX_VALID_TEMPERATURE_C = 180.0f;

// --- Steering angle (0x138) ---
constexpr float MAX_ABS_STEERING_DEGREES = 1000.0f;

// --- Lateral acceleration (0x13B) and brake (0x139): units not yet
// independently validated (see docs/can/signal-map.md). Start generous. ---
constexpr float MAX_ABS_LATERAL_ACCEL_SCALED = 60.0f;   // raw int8 * 0.2 -> +-25.4 max possible
constexpr float MAX_BRAKE_PRESSURE_SCALED = 32640.0f;   // raw uint8 * 128 max possible

// --- Freshness timeouts (ms since last valid frame for that signal) ---
constexpr uint32_t FAST_SIGNAL_STALE_MS = 250;
constexpr uint32_t FAST_SIGNAL_LOST_MS = 750;

constexpr uint32_t TEMPERATURE_STALE_MS = 2000;
constexpr uint32_t TEMPERATURE_LOST_MS = 5000;

constexpr uint32_t OIL_PRESSURE_STALE_MS = 250;
constexpr uint32_t OIL_PRESSURE_LOST_MS = 750;

// --- Engine-running hysteresis (RPM is the authoritative source) ---
constexpr uint16_t ENGINE_RUNNING_RPM_THRESHOLD = 400;
constexpr uint16_t ENGINE_STOPPED_RPM_THRESHOLD = 200;
constexpr uint32_t ENGINE_STATE_DEBOUNCE_MS = 500;

// Provisional engine-running fallback when RPM is invalid but pressure is
// valid. Marked provisional: pressure-based detection is not authoritative.
constexpr float PROVISIONAL_ENGINE_RUNNING_PSI = 3.0f;
constexpr uint32_t PROVISIONAL_ENGINE_RUNNING_DEBOUNCE_MS = 500;
