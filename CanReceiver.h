#pragma once

#include <stdint.h>

#include <driver/twai.h>

#include "SignalDecoders.h"

// Owns the ESP32 TWAI controller. Listen-only, 500 kbit/s, accept-all
// filter -- this ESP32 is a passive bus monitor and must never transmit a
// CAN frame. See docs/wiring.md for the safety rationale.
class CanReceiver {
public:
    struct BusHealth {
        uint32_t rxQueueOverflowCount = 0;
        uint32_t errorPassiveCount = 0;
        uint32_t busOffCount = 0;
        uint32_t busRecoveredCount = 0;
        bool busOff = false;
    };

    bool begin();

    // Non-blocking single-frame read. Returns false if nothing was queued.
    bool readFrame(twai_message_t &frame);

    // Drains and reacts to TWAI alerts (queue overflow, error-passive,
    // bus-off/recovery). Call once per loop. Recovery keeps listen-only
    // mode -- it never reconfigures into a transmit-capable mode.
    void handleAlerts(uint32_t nowMs);

    const BusHealth &busHealth() const { return health_; }

private:
    BusHealth health_;
    uint32_t lastRecoveryLogMs_ = 0;
};

// Trivial field copy from the ESP-IDF frame type to the host-testable
// CanFrame decoders operate on.
inline CanFrame twaiMessageToCanFrame(const twai_message_t &msg) {
    CanFrame frame;
    frame.identifier = msg.identifier;
    frame.extd = msg.extd;
    frame.rtr = msg.rtr;
    frame.data_length_code = msg.data_length_code;
    uint8_t len = frame.data_length_code > 8 ? 8 : frame.data_length_code;
    for (uint8_t i = 0; i < len; i++) frame.data[i] = msg.data[i];
    for (uint8_t i = len; i < 8; i++) frame.data[i] = 0;
    return frame;
}
