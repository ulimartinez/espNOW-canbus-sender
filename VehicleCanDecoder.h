#pragma once

#include <stdint.h>

#include "SignalDecoders.h"
#include "VehicleData.h"

// Decodes CanFrames into VehicleData, tracks per-signal freshness, and
// derives engine-running state from RPM (with a provisional pressure-based
// fallback). Takes CanFrame rather than twai_message_t so it has no
// ESP-IDF/Arduino dependency and can be unit tested on the host
// (tests/decoder_tests) -- CanReceiver does the trivial twai_message_t ->
// CanFrame field copy before calling processFrame().
class VehicleCanDecoder {
public:
    // Returns true if the frame updated at least one signal.
    bool processFrame(const CanFrame &frame, uint32_t nowMs);

    // Clears validity flags for any signal that has exceeded its "lost"
    // timeout. Call every loop iteration, not only when a frame arrives, so
    // a silent bus still ages signals out correctly.
    void updateFreshness(uint32_t nowMs);

    const VehicleData &data() const { return data_; }

    void reset();

private:
    void applyOilPressure(float psi, uint32_t nowMs);
    void applyRpm(uint16_t rpm, uint32_t nowMs);
    void applyAccelerator(float percent, uint32_t nowMs);
    void applyOilTemperature(float celsius, uint32_t nowMs);
    void applyCoolantTemperature(float celsius, uint32_t nowMs);
    void applySteeringAngle(float degrees, uint32_t nowMs);
    void applyLateralAcceleration(float scaled, uint32_t nowMs);
    void applyBrakePressure(float scaled, uint32_t nowMs);

    void updateEngineRunningState(uint32_t nowMs);

    VehicleData data_;

    // Engine-running hysteresis / debounce state. A single pending
    // candidate is shared between the RPM-based and pressure-fallback
    // paths since only one is active at a time (RPM preferred whenever
    // valid).
    bool pendingEngineRunning_ = false;
    uint32_t pendingEngineRunningSinceMs_ = 0;
    bool havePendingEngineRunning_ = false;
};
