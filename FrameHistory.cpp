#include "FrameHistory.h"

bool FrameHistory::isImportant(uint32_t id) {
    for (size_t i = 0; i < IMPORTANT_CAN_ID_COUNT; i++) {
        if (IMPORTANT_CAN_IDS[i] == id) return true;
    }
    return false;
}

void FrameHistory::record(const CanFrame &frame, uint32_t nowMs) {
    if (!isImportant(frame.identifier)) return;

    FrameHistoryEntry &e = buffer_[writeIndex_];
    e.timestampMs = nowMs;
    e.identifier = frame.identifier;
    e.dlc = frame.data_length_code;
    for (uint8_t i = 0; i < 8; i++) e.data[i] = frame.data[i];

    writeIndex_ = (writeIndex_ + 1) % IMPORTANT_FRAME_HISTORY_SIZE;
    if (count_ < IMPORTANT_FRAME_HISTORY_SIZE) count_++;
}

void FrameHistory::clear() {
    writeIndex_ = 0;
    count_ = 0;
}

const FrameHistoryEntry &FrameHistory::at(size_t indexFromOldest) const {
    size_t oldestIndex = (count_ < IMPORTANT_FRAME_HISTORY_SIZE) ? 0 : writeIndex_;
    size_t actual = (oldestIndex + indexFromOldest) % IMPORTANT_FRAME_HISTORY_SIZE;
    return buffer_[actual];
}
