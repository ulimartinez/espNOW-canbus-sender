#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CanConfig.h"
#include "SignalDecoders.h"

// Per-ID frame counters, approximate frequency, and observed byte ranges.
// Used by the `can summary` Serial command and decoder validation -- not
// on the hot path of any signal decode.
struct CanIdStats {
    uint32_t id = 0;
    uint32_t count = 0;
    uint32_t lastSeenMs = 0;
    uint32_t firstSeenMs = 0;
    uint8_t lastDlc = 0;
    uint8_t lastData[8] = {0};
    uint8_t minData[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t maxData[8] = {0};
    uint8_t changedMask = 0;  // bit i set if data[i] differed from the previous frame with this ID
};

class CanStatistics {
public:
    void recordFrame(const CanFrame &frame, uint32_t nowMs);
    void clear();

    uint32_t totalFrameCount() const { return totalFrameCount_; }
    float approximateBusFps(uint32_t nowMs) const;

    size_t idCount() const { return usedCount_; }
    const CanIdStats &idStatsAt(size_t index) const { return entries_[index]; }
    const CanIdStats *find(uint32_t id) const;

    // Prints a candump-style per-ID summary table to Serial (`can summary`).
    void printSummary() const;

private:
    CanIdStats *findOrCreate(uint32_t id);

    CanIdStats entries_[CAN_STATISTICS_MAX_IDS];
    size_t usedCount_ = 0;
    uint32_t totalFrameCount_ = 0;
    uint32_t firstFrameMs_ = 0;
    bool haveFirstFrame_ = false;
};
