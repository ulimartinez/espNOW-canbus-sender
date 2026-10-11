#include "PressureWarningLogic.h"

#include <stddef.h>

#include "SignalConfig.h"

namespace {

// Normal < Caution < Critical. Unknown/SensorError are handled separately
// by the debouncer (never debounced) and never passed to this.
uint8_t severity(PressureStatus status) {
    switch (status) {
        case PressureStatus::Normal: return 0;
        case PressureStatus::Caution: return 1;
        case PressureStatus::Critical: return 2;
        default: return 0;
    }
}

}  // namespace

const char *pressureStatusName(PressureStatus status) {
    switch (status) {
        case PressureStatus::Unknown: return "Unknown";
        case PressureStatus::Normal: return "Normal";
        case PressureStatus::Caution: return "Caution";
        case PressureStatus::Critical: return "Critical";
        case PressureStatus::SensorError: return "SensorError";
    }
    return "Unknown";
}

void interpolatePressureThresholds(uint16_t rpm, float &cautionPsi, float &criticalPsi) {
    const PressureThresholdPoint &first = TRACK_LOG_THRESHOLDS[0];
    const PressureThresholdPoint &last = TRACK_LOG_THRESHOLDS[TRACK_LOG_THRESHOLD_COUNT - 1];

    if (rpm <= first.rpm) {
        cautionPsi = first.cautionPsi;
        criticalPsi = first.criticalPsi;
        return;
    }
    if (rpm >= last.rpm) {
        cautionPsi = last.cautionPsi;
        criticalPsi = last.criticalPsi;
        return;
    }

    for (size_t i = 0; i + 1 < TRACK_LOG_THRESHOLD_COUNT; i++) {
        const PressureThresholdPoint &a = TRACK_LOG_THRESHOLDS[i];
        const PressureThresholdPoint &b = TRACK_LOG_THRESHOLDS[i + 1];
        if (rpm >= a.rpm && rpm <= b.rpm) {
            float progress = static_cast<float>(rpm - a.rpm) / static_cast<float>(b.rpm - a.rpm);
            cautionPsi = a.cautionPsi + (b.cautionPsi - a.cautionPsi) * progress;
            criticalPsi = a.criticalPsi + (b.criticalPsi - a.criticalPsi) * progress;
            return;
        }
    }

    // Unreachable given the bounds checks above.
    cautionPsi = last.cautionPsi;
    criticalPsi = last.criticalPsi;
}

PressureStatus evaluatePressureStatus(bool sensorError, bool pressureValid, float pressurePsi, bool rpmValid,
                                       uint16_t rpm, bool engineRunning) {
    if (sensorError) return PressureStatus::SensorError;
    if (!pressureValid) return PressureStatus::Unknown;

    // Never call a critical warning from an engine-off zero-pressure
    // reading -- that's expected, not a fault.
    if (!engineRunning) return PressureStatus::Normal;
    if (!rpmValid) return PressureStatus::Unknown;

    float cautionPsi, criticalPsi;
    interpolatePressureThresholds(rpm, cautionPsi, criticalPsi);

    if (pressurePsi < criticalPsi) return PressureStatus::Critical;
    if (pressurePsi < cautionPsi) return PressureStatus::Caution;
    return PressureStatus::Normal;
}

PressureStatus PressureWarningDebouncer::evaluate(bool sensorError, bool pressureValid, float pressurePsi,
                                                   bool rpmValid, uint16_t rpm, bool engineRunning, uint32_t nowMs) {
    PressureStatus instant = evaluatePressureStatus(sensorError, pressureValid, pressurePsi, rpmValid, rpm,
                                                     engineRunning);

    // Unknown/SensorError are data-validity facts, not a "is this corner
    // normal?" question -- propagate immediately in both directions.
    if (instant == PressureStatus::Unknown || instant == PressureStatus::SensorError) {
        committed_ = instant;
        havePending_ = false;
        return committed_;
    }

    // De-escalation (recovery to a less-or-equally severe status) is never
    // debounced -- a cleared warning should clear the instant pressure is
    // back to a safe level.
    if (severity(instant) <= severity(committed_)) {
        committed_ = instant;
        havePending_ = false;
        return committed_;
    }

    // Escalating: require the more-severe condition to persist for
    // PRESSURE_WARNING_DEBOUNCE_MS before committing to it.
    if (!havePending_ || pending_ != instant) {
        pending_ = instant;
        pendingSinceMs_ = nowMs;
        havePending_ = true;
        return committed_;
    }

    if (nowMs - pendingSinceMs_ >= PRESSURE_WARNING_DEBOUNCE_MS) {
        committed_ = pending_;
        havePending_ = false;
    }
    return committed_;
}

void PressureWarningDebouncer::reset() {
    committed_ = PressureStatus::Normal;
    havePending_ = false;
    pending_ = PressureStatus::Unknown;
    pendingSinceMs_ = 0;
}
