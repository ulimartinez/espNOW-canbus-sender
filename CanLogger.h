#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CanConfig.h"
#include "SignalDecoders.h"

enum class CanLogMode : uint8_t {
    Disabled,
    AllFrames,
    ChangedFrames,
    SelectedIds
};

// Raw/changed-frame candump-style Serial logging for CAN validation
// (`can start`/`can stop`/`can filter ...`). Intentionally independent of
// CanStatistics so logging can be toggled without affecting the counters
// used by `can summary`.
class CanLogger {
public:
    void setMode(CanLogMode mode);
    CanLogMode mode() const { return mode_; }

    void clearIdFilter();
    void addIdFilter(uint32_t id);

    // Call for every received frame; prints per the current mode's rules.
    // No-op (and cheap) when mode() == Disabled.
    void maybeLog(const CanFrame &frame, uint32_t nowMs);

private:
    bool idIsSelected(uint32_t id) const;
    bool hasChangedSincePrevious(const CanFrame &frame);
    void printFrame(const CanFrame &frame, uint32_t nowMs) const;

    CanLogMode mode_ = CanLogMode::Disabled;

    uint32_t selectedIds_[CAN_STATISTICS_MAX_IDS] = {0};
    size_t selectedIdCount_ = 0;

    // Last-seen payload per ID, used only for ChangedFrames mode.
    struct LastPayload {
        uint32_t id = 0;
        uint8_t data[8] = {0};
        uint8_t dlc = 0;
        bool seen = false;
    };
    LastPayload lastPayloads_[CAN_STATISTICS_MAX_IDS];
    size_t lastPayloadCount_ = 0;
};
