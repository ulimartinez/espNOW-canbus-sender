#include "CanStatistics.h"

#include <Arduino.h>

CanIdStats *CanStatistics::findOrCreate(uint32_t id) {
    for (size_t i = 0; i < usedCount_; i++) {
        if (entries_[i].id == id) return &entries_[i];
    }
    if (usedCount_ >= CAN_STATISTICS_MAX_IDS) {
        return nullptr;  // table full; drop stats for further new IDs rather than overrun
    }
    CanIdStats &e = entries_[usedCount_++];
    e = CanIdStats();
    e.id = id;
    return &e;
}

const CanIdStats *CanStatistics::find(uint32_t id) const {
    for (size_t i = 0; i < usedCount_; i++) {
        if (entries_[i].id == id) return &entries_[i];
    }
    return nullptr;
}

void CanStatistics::recordFrame(const CanFrame &frame, uint32_t nowMs) {
    if (!haveFirstFrame_) {
        haveFirstFrame_ = true;
        firstFrameMs_ = nowMs;
    }
    totalFrameCount_++;

    CanIdStats *e = findOrCreate(frame.identifier);
    if (!e) return;

    if (e->count == 0) e->firstSeenMs = nowMs;
    e->count++;
    e->lastSeenMs = nowMs;
    e->lastDlc = frame.data_length_code;

    uint8_t len = frame.data_length_code > 8 ? 8 : frame.data_length_code;
    e->changedMask = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = frame.data[i];
        if (e->count > 1 && b != e->lastData[i]) e->changedMask |= (1u << i);
        e->lastData[i] = b;
        if (b < e->minData[i]) e->minData[i] = b;
        if (b > e->maxData[i]) e->maxData[i] = b;
    }
}

void CanStatistics::clear() {
    usedCount_ = 0;
    totalFrameCount_ = 0;
    haveFirstFrame_ = false;
    firstFrameMs_ = 0;
}

float CanStatistics::approximateBusFps(uint32_t nowMs) const {
    if (!haveFirstFrame_ || nowMs <= firstFrameMs_) return 0.0f;
    float elapsedS = static_cast<float>(nowMs - firstFrameMs_) / 1000.0f;
    if (elapsedS <= 0.0f) return 0.0f;
    return static_cast<float>(totalFrameCount_) / elapsedS;
}

void CanStatistics::printSummary() const {
    Serial.printf("[CAN][SUMMARY] %u unique IDs, %lu total frames\n", (unsigned)usedCount_,
                  (unsigned long)totalFrameCount_);
    for (size_t i = 0; i < usedCount_; i++) {
        const CanIdStats &e = entries_[i];
        float hz = 0.0f;
        if (e.lastSeenMs > e.firstSeenMs) {
            hz = static_cast<float>(e.count - 1) * 1000.0f / static_cast<float>(e.lastSeenMs - e.firstSeenMs);
        }
        Serial.printf("  id=0x%03lX count=%-8lu hz=%-6.1f dlc=%u last=", (unsigned long)e.id,
                      (unsigned long)e.count, hz, e.lastDlc);
        for (uint8_t b = 0; b < e.lastDlc && b < 8; b++) Serial.printf("%02X ", e.lastData[b]);
        Serial.println();
    }
}
