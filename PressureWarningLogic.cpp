#include "PressureWarningLogic.h"

#include <stddef.h>

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
    const PressureThresholdPoint &first = DEVELOPMENT_THRESHOLDS[0];
    const PressureThresholdPoint &last = DEVELOPMENT_THRESHOLDS[DEVELOPMENT_THRESHOLD_COUNT - 1];

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

    for (size_t i = 0; i + 1 < DEVELOPMENT_THRESHOLD_COUNT; i++) {
        const PressureThresholdPoint &a = DEVELOPMENT_THRESHOLDS[i];
        const PressureThresholdPoint &b = DEVELOPMENT_THRESHOLDS[i + 1];
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
