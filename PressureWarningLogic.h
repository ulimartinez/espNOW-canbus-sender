#pragma once

#include <stddef.h>
#include <stdint.h>

// Supplemental gauge warning logic. The Ansix module already runs the
// factory oil-pressure warning independently of this firmware -- this is
// purely an additional display cue. DEVELOPMENT_THRESHOLDS below are
// placeholders tuned for "reasonable on the bench," NOT a validated
// engine-protection specification. Do not claim they reproduce any
// manufacturer algorithm.

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

// Development placeholders -- see docs/can/ansix-oil-pressure.md.
constexpr PressureThresholdPoint DEVELOPMENT_THRESHOLDS[] = {
    {800, 10.0f, 6.0f},
    {2000, 25.0f, 18.0f},
    {4000, 40.0f, 30.0f},
    {6000, 50.0f, 38.0f},
    {7500, 55.0f, 42.0f},
};
constexpr size_t DEVELOPMENT_THRESHOLD_COUNT =
    sizeof(DEVELOPMENT_THRESHOLDS) / sizeof(DEVELOPMENT_THRESHOLDS[0]);

// Linearly interpolates caution/critical PSI thresholds for the given RPM
// from DEVELOPMENT_THRESHOLDS, clamping to the table's endpoints outside
// its range.
void interpolatePressureThresholds(uint16_t rpm, float &cautionPsi, float &criticalPsi);

// Evaluates supplemental pressure status. Requires both pressure and RPM to
// be valid and RPM-dependent logic requires engineRunning so an engine-off
// zero-pressure reading never reports Critical. Returns Unknown when
// pressure is invalid (warning logic must not run on invalid data) or
// SensorError when sensorError is set.
PressureStatus evaluatePressureStatus(bool sensorError, bool pressureValid, float pressurePsi, bool rpmValid,
                                       uint16_t rpm, bool engineRunning);
