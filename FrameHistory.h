#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CanConfig.h"
#include "SignalDecoders.h"

struct FrameHistoryEntry {
    uint32_t timestampMs = 0;
    uint32_t identifier = 0;
    uint8_t dlc = 0;
    uint8_t data[8] = {0};
};

// Fixed-depth ring buffer of recent frames for the "important" CAN IDs this
// firmware decodes (see IMPORTANT_CAN_IDS in CanConfig.h). Deliberately
// does not store the entire bus -- just enough recent history for
// diagnostics/snapshot inspection and decoder validation.
class FrameHistory {
public:
    void record(const CanFrame &frame, uint32_t nowMs);
    void clear();

    size_t size() const { return count_; }

    // index 0 is the oldest retained entry, size()-1 the newest.
    const FrameHistoryEntry &at(size_t indexFromOldest) const;

private:
    static bool isImportant(uint32_t id);

    FrameHistoryEntry buffer_[IMPORTANT_FRAME_HISTORY_SIZE];
    size_t writeIndex_ = 0;
    size_t count_ = 0;
};
