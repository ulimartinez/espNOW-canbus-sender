#include "CanLogger.h"

#include <Arduino.h>

void CanLogger::setMode(CanLogMode mode) { mode_ = mode; }

void CanLogger::clearIdFilter() { selectedIdCount_ = 0; }

void CanLogger::addIdFilter(uint32_t id) {
    if (selectedIdCount_ >= CAN_STATISTICS_MAX_IDS) return;
    for (size_t i = 0; i < selectedIdCount_; i++) {
        if (selectedIds_[i] == id) return;  // already filtered
    }
    selectedIds_[selectedIdCount_++] = id;
}

bool CanLogger::idIsSelected(uint32_t id) const {
    for (size_t i = 0; i < selectedIdCount_; i++) {
        if (selectedIds_[i] == id) return true;
    }
    return false;
}

bool CanLogger::hasChangedSincePrevious(const CanFrame &frame) {
    for (size_t i = 0; i < lastPayloadCount_; i++) {
        LastPayload &p = lastPayloads_[i];
        if (p.id != frame.identifier) continue;

        bool changed = (p.dlc != frame.data_length_code);
        if (!changed) {
            for (uint8_t b = 0; b < frame.data_length_code && b < 8; b++) {
                if (p.data[b] != frame.data[b]) {
                    changed = true;
                    break;
                }
            }
        }
        p.dlc = frame.data_length_code;
        for (uint8_t b = 0; b < 8; b++) p.data[b] = frame.data[b];
        return changed;
    }

    // First time seeing this ID: always log it, and start tracking it.
    if (lastPayloadCount_ < CAN_STATISTICS_MAX_IDS) {
        LastPayload &p = lastPayloads_[lastPayloadCount_++];
        p.id = frame.identifier;
        p.dlc = frame.data_length_code;
        for (uint8_t b = 0; b < 8; b++) p.data[b] = frame.data[b];
        p.seen = true;
    }
    return true;
}

void CanLogger::printFrame(const CanFrame &frame, uint32_t nowMs) const {
    Serial.printf("[CAN] t=%lu id=0x%03lX %s dlc=%u data=", (unsigned long)nowMs, (unsigned long)frame.identifier,
                  frame.extd ? "ext" : "std", frame.data_length_code);
    for (uint8_t i = 0; i < frame.data_length_code && i < 8; i++) Serial.printf("%02X ", frame.data[i]);
    Serial.println();
}

void CanLogger::maybeLog(const CanFrame &frame, uint32_t nowMs) {
    switch (mode_) {
        case CanLogMode::Disabled:
            return;
        case CanLogMode::AllFrames:
            printFrame(frame, nowMs);
            return;
        case CanLogMode::ChangedFrames:
            if (hasChangedSincePrevious(frame)) printFrame(frame, nowMs);
            return;
        case CanLogMode::SelectedIds:
            if (idIsSelected(frame.identifier)) printFrame(frame, nowMs);
            return;
    }
}
