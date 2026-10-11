#pragma once

#include <stdint.h>

// Central decoded-signal state. Populated by VehicleCanDecoder (real CAN)
// or VehicleSimulator (wireless bench test) -- both feed the same
// structure so the rest of the pipeline (warning logic, packet builder,
// ESP-NOW sender) can't tell which source produced it. Keep raw CAN frames
// out of this struct; it holds only decoded, engineering-unit values.
struct VehicleData {
    float oilPressurePsi = 0.0f;
    float oilTemperatureC = 0.0f;
    float coolantTemperatureC = 0.0f;
    float acceleratorPercent = 0.0f;
    float steeringAngleDegrees = 0.0f;
    float lateralAccelerationMps2 = 0.0f;  // m/s^2 -- validated against track logs, see docs/can/signal-map.md
    float brakePressureScaled = 0.0f;        // unit not independently validated

    uint16_t rpm = 0;

    bool oilPressureValid = false;
    bool oilTemperatureValid = false;
    bool coolantTemperatureValid = false;
    bool acceleratorValid = false;
    bool steeringAngleValid = false;
    bool lateralAccelerationValid = false;
    bool brakePressureValid = false;
    bool rpmValid = false;

    bool engineRunning = false;
    bool engineRunningProvisional = false;  // true if engineRunning was derived from pressure, not RPM
    bool sensorError = false;

    uint32_t lastOilPressureMs = 0;
    uint32_t lastOilTemperatureMs = 0;
    uint32_t lastCoolantTemperatureMs = 0;
    uint32_t lastAcceleratorMs = 0;
    uint32_t lastSteeringAngleMs = 0;
    uint32_t lastLateralAccelerationMs = 0;
    uint32_t lastBrakePressureMs = 0;
    uint32_t lastRpmMs = 0;

    // Diagnostic-only counters (not transmitted).
    uint32_t invalidOilPressureCount = 0;
    uint32_t invalidRpmCount = 0;
    uint32_t invalidAcceleratorCount = 0;
    uint32_t invalidOilTemperatureCount = 0;
    uint32_t invalidCoolantTemperatureCount = 0;
    uint32_t invalidSteeringAngleCount = 0;
    uint32_t invalidLateralAccelerationCount = 0;
    uint32_t invalidBrakePressureCount = 0;
    uint32_t malformedFrameCount = 0;
};

inline float celsiusToFahrenheit(float valueC) {
    return valueC * 9.0f / 5.0f + 32.0f;
}
