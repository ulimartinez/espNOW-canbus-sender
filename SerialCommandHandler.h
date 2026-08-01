#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "CanLogger.h"
#include "CanReceiver.h"
#include "CanStatistics.h"
#include "EspNowSender.h"
#include "FrameHistory.h"
#include "GaugePacket.h"
#include "GaugePacketBuilder.h"
#include "VehicleCanDecoder.h"
#include "VehicleData.h"
#include "VehicleSimulator.h"

// Whichever subsystems exist in the current build (WIRELESS_BENCH_TEST_MODE
// vs. real CAN) are wired up here by main.ino; the rest stay nullptr. This
// lets one command handler serve both build modes without #ifdefs spread
// through its implementation.
struct SystemContext {
    // Shared monotonic GaugePacket sequence counter owned by main.ino;
    // shared so `test send_once`/`espnow test` can also draw from it.
    uint32_t *sequence = nullptr;

    VehicleCanDecoder *decoder = nullptr;
    CanReceiver *canReceiver = nullptr;
    CanStatistics *canStats = nullptr;
    CanLogger *canLogger = nullptr;
    FrameHistory *frameHistory = nullptr;

    VehicleSimulator *simulator = nullptr;

    EspNowSender *espNowSender = nullptr;
};

// Parses newline-terminated Serial commands (see README "Serial commands"
// section for the full list) and a manual fixed-value override mode useful
// for verifying exact display thresholds/layouts independent of the
// automatic bench-test scenario.
class SerialCommandHandler {
public:
    void poll(SystemContext &ctx, uint32_t nowMs);

    // When active, overrides fields in `data` in place before packet
    // building. Stays active until another `set` command, `test restart`,
    // or reboot.
    bool manualOverrideActive() const { return manualOverrideActive_; }
    void applyManualOverride(VehicleData &data) const;

    // Independent of manualOverrideActive_: forces (or leaves alone) the
    // packet's low-pressure warning bit. Call after GaugePacketBuilder and
    // re-sign, since the CRC covers the flags field.
    bool manualWarningOverrideActive() const { return manualWarningOverrideActive_; }
    void applyManualWarningOverride(GaugePacket &packet) const;

    // True while `test stop` has paused scheduled transmission (bench-test
    // mode only). Distinct from the simulator's own WirelessLoss stage.
    bool transmitPaused() const { return testTransmitPaused_; }

    // Shared by the `status` command and main.ino's periodic [STATUS] line
    // so there's one implementation of the summary format.
    void printStatusLine(SystemContext &ctx, uint32_t nowMs) const;

private:
    void handleLine(const String &line, SystemContext &ctx, uint32_t nowMs);

    void cmdHelp() const;
    void cmdStatus(SystemContext &ctx, uint32_t nowMs) const;
    void cmdMac() const;
    void cmdCan(const String &args, SystemContext &ctx) const;
    void cmdMark(const String &label, uint32_t nowMs) const;
    void cmdEspNow(const String &args, SystemContext &ctx, uint32_t nowMs);
    void cmdTest(const String &args, SystemContext &ctx, uint32_t nowMs);
    void cmdSet(const String &args, SystemContext &ctx);
    void cmdSendOnce(SystemContext &ctx, uint32_t nowMs) const;

    static VehicleData currentSourceData(SystemContext &ctx);

    String lineBuffer_;

    bool manualOverrideActive_ = false;
    VehicleData manualOverride_;

    bool manualWarningOverrideActive_ = false;
    bool manualWarningOverrideValue_ = false;

    bool testTransmitPaused_ = false;
};
