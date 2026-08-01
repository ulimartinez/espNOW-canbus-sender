#pragma once

#include <stdint.h>

#include "VehicleData.h"

// Wireless bench-test data source (WIRELESS_BENCH_TEST_MODE). Generates a
// repeating ~50s scenario that exercises every display state -- see
// docs/validation-plan.md for the full stage table -- and feeds it through
// VehicleData exactly like a real decoded CAN frame would, so the rest of
// the pipeline (PressureWarningLogic, GaugePacketBuilder, EspNowSender)
// runs identically to production. Never constructs a GaugePacket directly.
//
// Values are deterministic functions of elapsed time (sine-wave "wobble",
// no random()) so a bench run is reproducible run to run.
class VehicleSimulator {
public:
    void begin(uint32_t nowMs);

    void update(uint32_t nowMs);

    const VehicleData &data() const { return data_; }

    // False only during the simulated wireless-loss stage: the caller must
    // skip calling EspNowSender::send() entirely that tick (not send an
    // empty/invalid packet) so the display's own stale-packet timeout is
    // what's being exercised.
    bool shouldTransmit() const { return shouldTransmit_; }

    const char *currentStageName() const;

    void restart(uint32_t nowMs);

    // Serial `test next` / `test previous` support.
    void advanceToNextStage(uint32_t nowMs);
    void advanceToPreviousStage(uint32_t nowMs);

    uint32_t stageElapsedMs(uint32_t nowMs) const { return nowMs - stageStartMs_; }
    uint32_t stageDurationMs() const;

private:
    enum class Stage : uint8_t {
        NoVehicleData,
        IgnitionOnEngineOff,
        EngineStart,
        ColdIdle,
        WarmUp,
        Acceleration,
        HighRpmNormal,
        CautionPressureEvent,
        CriticalLowPressureEvent,
        Recovery,
        SensorError,
        CanDataLoss,
        WirelessLoss,
        SenderRecovery,
        EngineShutdown,
        StageCount
    };

    void enterStage(Stage stage, uint32_t nowMs);
    void computeStageData(Stage stage, uint32_t elapsedMs, uint32_t nowMs);
    static uint32_t durationForStage(Stage stage);

    VehicleData data_;
    Stage stage_ = Stage::NoVehicleData;
    uint32_t stageStartMs_ = 0;
    bool shouldTransmit_ = true;
};
