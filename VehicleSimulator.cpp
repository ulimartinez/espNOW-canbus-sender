#include "VehicleSimulator.h"

#include <math.h>

namespace {

float lerp(float start, float end, float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return start + (end - start) * progress;
}

// Deterministic "liveliness" -- two fixed-period sine waves summed, no
// random(), so a bench run is reproducible.
float wobble(uint32_t nowMs, float amplitude) {
    float t = static_cast<float>(nowMs);
    return amplitude * (0.7f * sinf(t / 400.0f) + 0.3f * sinf(t / 137.0f));
}

float fahrenheitToCelsius(float f) {
    return (f - 32.0f) * 5.0f / 9.0f;
}

}  // namespace

uint32_t VehicleSimulator::durationForStage(Stage stage) {
    switch (stage) {
        case Stage::NoVehicleData: return 3000;
        case Stage::IgnitionOnEngineOff: return 3000;
        case Stage::EngineStart: return 2000;
        case Stage::ColdIdle: return 5000;
        case Stage::WarmUp: return 8000;
        case Stage::Acceleration: return 6000;
        case Stage::HighRpmNormal: return 4000;
        case Stage::CautionPressureEvent: return 3000;
        case Stage::CriticalLowPressureEvent: return 3000;
        case Stage::Recovery: return 4000;
        case Stage::SensorError: return 3000;
        case Stage::CanDataLoss: return 3000;
        // The display's DATA_LOST_TIMEOUT_MS is 1000ms; hold ~2s past it.
        case Stage::WirelessLoss: return 3000;
        case Stage::SenderRecovery: return 3000;
        case Stage::EngineShutdown: return 4000;
        default: return 3000;
    }
}

uint32_t VehicleSimulator::stageDurationMs() const { return durationForStage(stage_); }

const char *VehicleSimulator::currentStageName() const {
    switch (stage_) {
        case Stage::NoVehicleData: return "NO_VEHICLE_DATA";
        case Stage::IgnitionOnEngineOff: return "IGNITION_ON_ENGINE_OFF";
        case Stage::EngineStart: return "ENGINE_START";
        case Stage::ColdIdle: return "COLD_IDLE";
        case Stage::WarmUp: return "WARM_UP";
        case Stage::Acceleration: return "ACCELERATION";
        case Stage::HighRpmNormal: return "HIGH_RPM_NORMAL";
        case Stage::CautionPressureEvent: return "CAUTION_PRESSURE";
        case Stage::CriticalLowPressureEvent: return "CRITICAL_LOW_PRESSURE";
        case Stage::Recovery: return "RECOVERY";
        case Stage::SensorError: return "SENSOR_ERROR";
        case Stage::CanDataLoss: return "CAN_DATA_LOSS";
        case Stage::WirelessLoss: return "WIRELESS_LOSS";
        case Stage::SenderRecovery: return "SENDER_RECOVERY";
        case Stage::EngineShutdown: return "ENGINE_SHUTDOWN";
        default: return "UNKNOWN";
    }
}

void VehicleSimulator::enterStage(Stage stage, uint32_t nowMs) {
    stage_ = stage;
    stageStartMs_ = nowMs;
    shouldTransmit_ = (stage != Stage::WirelessLoss);
}

void VehicleSimulator::begin(uint32_t nowMs) {
    data_ = VehicleData();
    enterStage(Stage::NoVehicleData, nowMs);
    computeStageData(stage_, 0, nowMs);
}

void VehicleSimulator::restart(uint32_t nowMs) { begin(nowMs); }

void VehicleSimulator::advanceToNextStage(uint32_t nowMs) {
    uint8_t next = (static_cast<uint8_t>(stage_) + 1) % static_cast<uint8_t>(Stage::StageCount);
    enterStage(static_cast<Stage>(next), nowMs);
    computeStageData(stage_, 0, nowMs);
}

void VehicleSimulator::advanceToPreviousStage(uint32_t nowMs) {
    uint8_t count = static_cast<uint8_t>(Stage::StageCount);
    uint8_t prev = (static_cast<uint8_t>(stage_) + count - 1) % count;
    enterStage(static_cast<Stage>(prev), nowMs);
    computeStageData(stage_, 0, nowMs);
}

void VehicleSimulator::update(uint32_t nowMs) {
    uint32_t elapsed = nowMs - stageStartMs_;
    if (elapsed >= durationForStage(stage_)) {
        uint8_t next = (static_cast<uint8_t>(stage_) + 1) % static_cast<uint8_t>(Stage::StageCount);
        enterStage(static_cast<Stage>(next), nowMs);
        elapsed = 0;
    }
    computeStageData(stage_, elapsed, nowMs);
}

void VehicleSimulator::computeStageData(Stage stage, uint32_t elapsedMs, uint32_t nowMs) {
    uint32_t durationMs = durationForStage(stage);
    float progress = durationMs > 0 ? static_cast<float>(elapsedMs) / static_cast<float>(durationMs) : 1.0f;

    // Defaults for this tick; each stage below overrides what it needs.
    VehicleData d = data_;
    d.sensorError = false;

    switch (stage) {
        case Stage::NoVehicleData: {
            d.oilPressureValid = false;
            d.rpmValid = false;
            d.oilTemperatureValid = false;
            d.engineRunning = false;
            break;
        }

        case Stage::IgnitionOnEngineOff: {
            d.oilPressurePsi = 0.0f;
            d.rpm = 0;
            d.oilTemperatureC = fahrenheitToCelsius(70.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = false;
            break;
        }

        case Stage::EngineStart: {
            d.rpm = static_cast<uint16_t>(lerp(0.0f, 1400.0f, progress));
            d.oilPressurePsi = lerp(0.0f, 75.0f, progress);
            d.oilTemperatureC = fahrenheitToCelsius(70.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::ColdIdle: {
            d.rpm = static_cast<uint16_t>(1250.0f + wobble(nowMs, 150.0f));
            d.oilPressurePsi = 70.0f + wobble(nowMs, 10.0f);
            d.oilTemperatureC = fahrenheitToCelsius(85.0f + wobble(nowMs, 15.0f));
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::WarmUp: {
            d.rpm = static_cast<uint16_t>(900.0f + wobble(nowMs, 50.0f));
            d.oilPressurePsi = lerp(40.0f, 25.0f, progress) + wobble(nowMs, 5.0f);
            d.oilTemperatureC = fahrenheitToCelsius(lerp(100.0f, 190.0f, progress));
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::Acceleration: {
            d.rpm = static_cast<uint16_t>(lerp(1000.0f, 7200.0f, progress));
            d.oilPressurePsi = lerp(30.0f, 85.0f, progress);
            d.oilTemperatureC = fahrenheitToCelsius(lerp(190.0f, 215.0f, progress));
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::HighRpmNormal: {
            d.rpm = static_cast<uint16_t>(6950.0f + wobble(nowMs, 300.0f));
            d.oilPressurePsi = 75.0f + wobble(nowMs, 10.0f);
            d.oilTemperatureC = fahrenheitToCelsius(222.0f + wobble(nowMs, 5.0f));
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::CautionPressureEvent: {
            d.rpm = 6500;
            d.oilPressurePsi = 45.0f;
            d.oilTemperatureC = fahrenheitToCelsius(224.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::CriticalLowPressureEvent: {
            d.rpm = 6500;
            d.oilPressurePsi = lerp(45.0f, 22.0f, fminf(progress * 2.0f, 1.0f));
            d.oilTemperatureC = fahrenheitToCelsius(224.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::Recovery: {
            d.rpm = static_cast<uint16_t>(lerp(6500.0f, 5000.0f, progress));
            d.oilPressurePsi = lerp(22.0f, 65.0f, progress);
            d.oilTemperatureC = fahrenheitToCelsius(220.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::SensorError: {
            d.rpm = static_cast<uint16_t>(3000.0f + wobble(nowMs, 200.0f));
            d.oilTemperatureC = fahrenheitToCelsius(220.0f);
            d.oilPressureValid = false;  // never transmit a fabricated pressure as valid
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.sensorError = true;
            d.engineRunning = true;
            break;
        }

        case Stage::CanDataLoss: {
            d.oilPressureValid = false;
            d.rpmValid = false;
            d.oilTemperatureValid = false;
            d.sensorError = false;
            d.engineRunning = false;
            break;
        }

        case Stage::WirelessLoss: {
            // Simulator keeps "running" internally (deterministic idle
            // data) but shouldTransmit_ is false for this whole stage --
            // the caller must skip send() entirely, not transmit this.
            d.rpm = static_cast<uint16_t>(900.0f + wobble(nowMs, 30.0f));
            d.oilPressurePsi = 30.0f + wobble(nowMs, 5.0f);
            d.oilTemperatureC = fahrenheitToCelsius(220.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::SenderRecovery: {
            d.rpm = 900;
            d.oilPressurePsi = 30.0f;
            d.oilTemperatureC = fahrenheitToCelsius(220.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = true;
            break;
        }

        case Stage::EngineShutdown: {
            d.rpm = static_cast<uint16_t>(lerp(900.0f, 0.0f, progress));
            d.oilPressurePsi = lerp(30.0f, 0.0f, progress);
            d.oilTemperatureC = fahrenheitToCelsius(210.0f);
            d.oilPressureValid = true;
            d.rpmValid = true;
            d.oilTemperatureValid = true;
            d.engineRunning = progress < 0.95f;
            break;
        }

        default:
            break;
    }

    // Protocol v2 fields, derived from the stage's own values so every
    // stage stays consistent without repeating them above. Coolant leads
    // oil temperature during warm-up (as on the real car) and settles at
    // thermostat temperature; lateral acceleration swings both directions
    // only during the driving stages.
    d.coolantTemperatureValid = d.oilTemperatureValid;
    if (d.oilTemperatureValid) {
        float oilF = celsiusToFahrenheit(d.oilTemperatureC);
        float coolantF = oilF;
        if (d.engineRunning) coolantF = fminf(oilF + 30.0f, 195.0f);
        if (oilF >= 195.0f) coolantF = 195.0f + wobble(nowMs, 2.0f);
        d.coolantTemperatureC = fahrenheitToCelsius(coolantF);
    }

    bool driving = (stage == Stage::Acceleration || stage == Stage::HighRpmNormal ||
                    stage == Stage::CautionPressureEvent || stage == Stage::CriticalLowPressureEvent ||
                    stage == Stage::Recovery);
    d.lateralAccelerationValid = d.rpmValid;
    d.lateralAccelerationMps2 = driving ? 12.0f * sinf(static_cast<float>(nowMs) / 900.0f) : wobble(nowMs, 0.2f);

    data_ = d;
}
