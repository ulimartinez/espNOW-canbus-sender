#include "VehicleCanDecoder.h"

#include "CanConfig.h"
#include "SignalConfig.h"

namespace {

inline void expire(bool &valid, uint32_t lastMs, uint32_t nowMs, uint32_t lostMs) {
    if (valid && (nowMs - lastMs) > lostMs) {
        valid = false;
    }
}

inline bool inRange(float value, float lo, float hi) {
    return value >= lo && value <= hi;
}

}  // namespace

void VehicleCanDecoder::applyOilPressure(float psi, uint32_t nowMs) {
    if (!inRange(psi, MIN_VALID_OIL_PRESSURE_PSI, MAX_VALID_OIL_PRESSURE_PSI)) {
        // Implausible value with a well-formed frame most likely means the
        // sensor/wiring itself is faulty, not just "no data" -- surface it
        // distinctly from staleness via sensorError (see updateFreshness).
        data_.oilPressureValid = false;
        data_.sensorError = true;
        data_.invalidOilPressureCount++;
        return;
    }

    data_.oilPressurePsi = psi;
    data_.oilPressureValid = true;
    data_.sensorError = false;
    data_.lastOilPressureMs = nowMs;
}

void VehicleCanDecoder::applyRpm(uint16_t rpm, uint32_t nowMs) {
    if (rpm < MIN_VALID_RPM || rpm > MAX_VALID_RPM) {
        data_.rpmValid = false;
        data_.invalidRpmCount++;
        return;
    }

    data_.rpm = rpm;
    data_.rpmValid = true;
    data_.lastRpmMs = nowMs;
}

void VehicleCanDecoder::applyAccelerator(float percent, uint32_t nowMs) {
    if (!inRange(percent, MIN_VALID_ACCELERATOR_PERCENT, MAX_VALID_ACCELERATOR_PERCENT)) {
        data_.acceleratorValid = false;
        data_.invalidAcceleratorCount++;
        return;
    }

    data_.acceleratorPercent = percent;
    data_.acceleratorValid = true;
    data_.lastAcceleratorMs = nowMs;
}

void VehicleCanDecoder::applyOilTemperature(float celsius, uint32_t nowMs) {
    if (!inRange(celsius, MIN_VALID_TEMPERATURE_C, MAX_VALID_TEMPERATURE_C)) {
        data_.oilTemperatureValid = false;
        data_.invalidOilTemperatureCount++;
        return;
    }

    data_.oilTemperatureC = celsius;
    data_.oilTemperatureValid = true;
    data_.lastOilTemperatureMs = nowMs;
}

void VehicleCanDecoder::applyCoolantTemperature(float celsius, uint32_t nowMs) {
    if (!inRange(celsius, MIN_VALID_TEMPERATURE_C, MAX_VALID_TEMPERATURE_C)) {
        data_.coolantTemperatureValid = false;
        data_.invalidCoolantTemperatureCount++;
        return;
    }

    data_.coolantTemperatureC = celsius;
    data_.coolantTemperatureValid = true;
    data_.lastCoolantTemperatureMs = nowMs;
}

void VehicleCanDecoder::applySteeringAngle(float degrees, uint32_t nowMs) {
    if (degrees < -MAX_ABS_STEERING_DEGREES || degrees > MAX_ABS_STEERING_DEGREES) {
        data_.steeringAngleValid = false;
        data_.invalidSteeringAngleCount++;
        return;
    }

    data_.steeringAngleDegrees = degrees;
    data_.steeringAngleValid = true;
    data_.lastSteeringAngleMs = nowMs;
}

void VehicleCanDecoder::applyLateralAcceleration(float scaled, uint32_t nowMs) {
    if (scaled < -MAX_ABS_LATERAL_ACCEL_MPS2 || scaled > MAX_ABS_LATERAL_ACCEL_MPS2) {
        data_.lateralAccelerationValid = false;
        data_.invalidLateralAccelerationCount++;
        return;
    }

    data_.lateralAccelerationMps2 = scaled;
    data_.lateralAccelerationValid = true;
    data_.lastLateralAccelerationMs = nowMs;
}

void VehicleCanDecoder::applyBrakePressure(float scaled, uint32_t nowMs) {
    if (scaled < 0.0f || scaled > MAX_BRAKE_PRESSURE_SCALED) {
        data_.brakePressureValid = false;
        data_.invalidBrakePressureCount++;
        return;
    }

    data_.brakePressureScaled = scaled;
    data_.brakePressureValid = true;
    data_.lastBrakePressureMs = nowMs;
}

void VehicleCanDecoder::updateEngineRunningState(uint32_t nowMs) {
    bool haveCandidate = false;
    bool candidate = data_.engineRunning;
    bool provisional = false;

    if (data_.rpmValid) {
        if (data_.rpm >= ENGINE_RUNNING_RPM_THRESHOLD) {
            candidate = true;
            haveCandidate = true;
        } else if (data_.rpm <= ENGINE_STOPPED_RPM_THRESHOLD) {
            candidate = false;
            haveCandidate = true;
        }
        // Between the two thresholds: hysteresis dead zone, no new candidate.
    } else if (data_.oilPressureValid) {
        // Provisional fallback only: pressure alone cannot reliably
        // distinguish "engine running" from e.g. residual line pressure.
        candidate = data_.oilPressurePsi > PROVISIONAL_ENGINE_RUNNING_PSI;
        haveCandidate = true;
        provisional = true;
    }

    if (!haveCandidate) {
        havePendingEngineRunning_ = false;
        return;
    }

    if (candidate == data_.engineRunning) {
        havePendingEngineRunning_ = false;
        data_.engineRunningProvisional = provisional;
        return;
    }

    const uint32_t debounceMs = provisional ? PROVISIONAL_ENGINE_RUNNING_DEBOUNCE_MS : ENGINE_STATE_DEBOUNCE_MS;

    if (!havePendingEngineRunning_ || pendingEngineRunning_ != candidate) {
        havePendingEngineRunning_ = true;
        pendingEngineRunning_ = candidate;
        pendingEngineRunningSinceMs_ = nowMs;
        return;
    }

    if (nowMs - pendingEngineRunningSinceMs_ >= debounceMs) {
        data_.engineRunning = candidate;
        data_.engineRunningProvisional = provisional;
        havePendingEngineRunning_ = false;
    }
}

bool VehicleCanDecoder::processFrame(const CanFrame &frame, uint32_t nowMs) {
    bool updatedAny = false;

    switch (frame.identifier) {
        case CAN_ID_OIL_PRESSURE: {
            float psi;
            if (decodeOilPressure(frame, psi)) {
                applyOilPressure(psi, nowMs);
                updatedAny = true;
            } else {
                data_.malformedFrameCount++;
            }
            break;
        }

        case CAN_ID_ENGINE: {
            uint16_t rpm;
            float accel;
            bool any = false;
            if (decodeEngineRpm(frame, rpm)) {
                applyRpm(rpm, nowMs);
                any = true;
            }
            if (decodeAcceleratorPercent(frame, accel)) {
                applyAccelerator(accel, nowMs);
                any = true;
            }
            if (!any) data_.malformedFrameCount++;
            updatedAny = any;
            break;
        }

        case CAN_ID_TEMPERATURE: {
            float oilC, coolantC;
            bool any = false;
            if (decodeOilTemperatureC(frame, oilC)) {
                applyOilTemperature(oilC, nowMs);
                any = true;
            }
            if (decodeCoolantTemperatureC(frame, coolantC)) {
                applyCoolantTemperature(coolantC, nowMs);
                any = true;
            }
            if (!any) data_.malformedFrameCount++;
            updatedAny = any;
            break;
        }

        case CAN_ID_STEERING: {
            float degrees;
            if (decodeSteeringAngleDegrees(frame, degrees)) {
                applySteeringAngle(degrees, nowMs);
                updatedAny = true;
            } else {
                data_.malformedFrameCount++;
            }
            break;
        }

        case CAN_ID_BRAKE: {
            float scaled;
            if (decodeBrakePressureScaled(frame, scaled)) {
                applyBrakePressure(scaled, nowMs);
                updatedAny = true;
            } else {
                data_.malformedFrameCount++;
            }
            break;
        }

        case CAN_ID_LATERAL_ACCEL: {
            float scaled;
            if (decodeLateralAcceleration(frame, scaled)) {
                applyLateralAcceleration(scaled, nowMs);
                updatedAny = true;
            } else {
                data_.malformedFrameCount++;
            }
            break;
        }

        default:
            return false;  // not a frame this decoder cares about
    }

    if (updatedAny) updateEngineRunningState(nowMs);
    return updatedAny;
}

void VehicleCanDecoder::updateFreshness(uint32_t nowMs) {
    expire(data_.oilPressureValid, data_.lastOilPressureMs, nowMs, OIL_PRESSURE_LOST_MS);
    if (!data_.oilPressureValid) {
        // A signal that has gone silent is a comms/staleness problem, not
        // (necessarily) the implausible-value case sensorError represents.
        data_.sensorError = false;
    }
    expire(data_.rpmValid, data_.lastRpmMs, nowMs, FAST_SIGNAL_LOST_MS);
    expire(data_.acceleratorValid, data_.lastAcceleratorMs, nowMs, FAST_SIGNAL_LOST_MS);
    expire(data_.steeringAngleValid, data_.lastSteeringAngleMs, nowMs, FAST_SIGNAL_LOST_MS);
    expire(data_.lateralAccelerationValid, data_.lastLateralAccelerationMs, nowMs, FAST_SIGNAL_LOST_MS);
    expire(data_.brakePressureValid, data_.lastBrakePressureMs, nowMs, FAST_SIGNAL_LOST_MS);
    expire(data_.oilTemperatureValid, data_.lastOilTemperatureMs, nowMs, TEMPERATURE_LOST_MS);
    expire(data_.coolantTemperatureValid, data_.lastCoolantTemperatureMs, nowMs, TEMPERATURE_LOST_MS);

    updateEngineRunningState(nowMs);
}

void VehicleCanDecoder::reset() {
    data_ = VehicleData();
    havePendingEngineRunning_ = false;
}
