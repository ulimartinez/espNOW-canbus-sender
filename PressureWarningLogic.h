#pragma once

#include <stddef.h>
#include <stdint.h>

// Supplemental gauge warning logic. The Ansix module already runs the
// factory oil-pressure warning independently of this firmware -- this is
// purely an additional display cue. TRACK_LOG_THRESHOLDS below are derived
// from real logged track sessions on this specific car (see
// docs/can/pressure-threshold-derivation.md for the full methodology and
// data) -- an upgrade in rigor over an arbitrary bench guess, but still NOT
// a validated engine-protection specification and NOT a substitute for the
// Ansix module's own warning. Do not claim they reproduce any manufacturer
// algorithm, and do not assume they transfer to a different car/engine/oil
// pan without re-deriving from that car's own logs.

enum class PressureStatus : uint8_t {
    Unknown,
    Normal,
    Caution,
    Critical,
    SensorError
};

const char *pressureStatusName(PressureStatus status);

struct PressureThresholdPoint {
    uint16_t rpm;
    float cautionPsi;
    float criticalPsi;
};

// Derived from RPM-bucketed percentiles and dip-event analysis of two
// RaceChrono/OBDLink MX+ track logs on this car (2025, before an oil pan
// upgrade; 2026, after) -- see docs/can/pressure-threshold-derivation.md.
// The 800/2000 RPM points are unchanged bench placeholders: neither log
// contains idle/low-RPM data to derive them from.
constexpr PressureThresholdPoint TRACK_LOG_THRESHOLDS[] = {
    {800, 10.0f, 6.0f},
    {2000, 25.0f, 18.0f},
    {4000, 28.0f, 15.0f},
    {5000, 25.0f, 15.0f},
    {6000, 32.0f, 20.0f},
    {6500, 28.0f, 18.0f},
    {7000, 32.0f, 22.0f},
    {7500, 42.0f, 32.0f},
};
constexpr size_t TRACK_LOG_THRESHOLD_COUNT =
    sizeof(TRACK_LOG_THRESHOLDS) / sizeof(TRACK_LOG_THRESHOLDS[0]);

// Linearly interpolates caution/critical PSI thresholds for the given RPM
// from TRACK_LOG_THRESHOLDS, clamping to the table's endpoints outside its
// range. The curve is intentionally non-monotonic (it dips in the
// 5000-6500 RPM band) -- that reflects a real, RPM-localized oil-surge
// tendency on this car, not a bug.
void interpolatePressureThresholds(uint16_t rpm, float &cautionPsi, float &criticalPsi);

// Evaluates instantaneous supplemental pressure status from the latest
// sample only (no debounce -- see PressureWarningDebouncer for the
// stateful, time-aware version actually used by GaugePacketBuilder).
// Requires both pressure and RPM to be valid, and RPM-dependent logic
// requires engineRunning so an engine-off zero-pressure reading never
// reports Critical. Returns Unknown when pressure is invalid (warning
// logic must not run on invalid data) or SensorError when sensorError is
// set.
PressureStatus evaluatePressureStatus(bool sensorError, bool pressureValid, float pressurePsi, bool rpmValid,
                                       uint16_t rpm, bool engineRunning);

// Stateful wrapper around evaluatePressureStatus() that debounces
// escalation (Normal -> Caution -> Critical) by PRESSURE_WARNING_DEBOUNCE_MS
// so brief, normal hard-cornering pressure transients don't flicker the
// warning -- see docs/can/pressure-threshold-derivation.md for why. De-
// escalation (recovery to a less severe status) and the Unknown/SensorError
// states are never debounced: those aren't the "is this corner normal?"
// question this class exists to answer, and clearing a warning -- or
// surfacing a genuine data/sensor fault -- should never be delayed.
class PressureWarningDebouncer {
public:
    PressureStatus evaluate(bool sensorError, bool pressureValid, float pressurePsi, bool rpmValid, uint16_t rpm,
                             bool engineRunning, uint32_t nowMs);

    PressureStatus status() const { return committed_; }

    void reset();

private:
    // Bootstraps as Normal (not Unknown) so escalation is debounced from
    // the very first sample too -- "assume fine until proven otherwise for
    // the debounce window," not a separate "no data yet" state.
    PressureStatus committed_ = PressureStatus::Normal;

    bool havePending_ = false;
    PressureStatus pending_ = PressureStatus::Unknown;
    uint32_t pendingSinceMs_ = 0;
};
