#pragma once

#include <stddef.h>
#include <stdint.h>

// TWAI (CAN) controller configuration. Listen-only, 500 kbit/s, per the
// vehicle bus. See docs/wiring.md for the termination and safety notes —
// this ESP32 must never transmit on the vehicle CAN network.

constexpr uint32_t CAN_BITRATE_BPS = 500000;

// Receive queue depth. Vehicle CAN traffic at 500kbit/s with many IDs can
// burst well above what one loop() iteration drains; keep this generous.
constexpr uint32_t CAN_RX_QUEUE_LEN = 64;

// Bounds how many frames processFrame() consumes per loop() call so a CAN
// burst can never block the ESP-NOW send schedule or Serial handling.
constexpr uint16_t CAN_MAX_FRAMES_PER_LOOP = 32;

// Important CAN IDs this firmware decodes. Used by CanLogger/FrameHistory
// to scope "important-frame" tracking without storing the entire bus.
constexpr uint32_t CAN_ID_ENGINE = 0x040;
constexpr uint32_t CAN_ID_STEERING = 0x138;
constexpr uint32_t CAN_ID_BRAKE = 0x139;
constexpr uint32_t CAN_ID_LATERAL_ACCEL = 0x13B;
constexpr uint32_t CAN_ID_OIL_PRESSURE = 0x171;
constexpr uint32_t CAN_ID_TEMPERATURE = 0x345;

constexpr uint32_t IMPORTANT_CAN_IDS[] = {
    CAN_ID_ENGINE,   CAN_ID_STEERING,       CAN_ID_BRAKE,
    CAN_ID_LATERAL_ACCEL, CAN_ID_OIL_PRESSURE, CAN_ID_TEMPERATURE,
};
constexpr size_t IMPORTANT_CAN_ID_COUNT = sizeof(IMPORTANT_CAN_IDS) / sizeof(IMPORTANT_CAN_IDS[0]);

constexpr size_t IMPORTANT_FRAME_HISTORY_SIZE = 256;
constexpr size_t CAN_STATISTICS_MAX_IDS = 64;
