#pragma once

#include <stdint.h>

// ESP-NOW sender configuration. Must match the display's NetworkConfig.h
// (../espNOW-car-gauge/NetworkConfig.h): same channel, and this sender's
// real MAC must eventually be entered as the display's EXPECTED_SENDER_MAC.

// Replace with the display's real MAC address. Print it from the display's
// Serial boot log ("[BOOT] Local MAC: ...") and enter it here. Until this is
// set, packets are unicast to all-zero and will not reach the display.
constexpr uint8_t DISPLAY_MAC[6] = {
    0x14, 0xC1, 0x9F, 0x26, 0x74, 0x28
};

// Must match ESPNOW_CHANNEL in the display's NetworkConfig.h.
constexpr uint8_t ESPNOW_CHANNEL = 1;

// Recovery behavior when sends start failing (e.g. display powered off,
// out of range, peer table corrupted).
constexpr uint32_t ESPNOW_RECOVERY_INTERVAL_MS = 5000;
constexpr uint16_t ESPNOW_FAILURES_BEFORE_REINIT = 50;
