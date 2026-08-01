#pragma once

#include <stdint.h>

#include "GaugePacket.h"

// ESP-NOW transport for GaugePacket. Used identically by the real CAN
// pipeline and the wireless bench-test simulator (WIRELESS_BENCH_TEST_MODE)
// -- neither builds a second, simplified packet path.
//
// The registered send callback runs in the WiFi/LWIP task context. Per the
// project's ESP-NOW callback rules it only updates plain counters here; it
// never prints, blocks, re-initializes WiFi, or retries. Recovery (peer
// re-registration after repeated failures) happens from maintain(), called
// from the main loop.
class EspNowSender {
public:
    struct Stats {
        uint32_t attempted = 0;
        uint32_t succeeded = 0;
        uint32_t failed = 0;
        uint16_t consecutiveFailures = 0;
        uint32_t lastSendAtMs = 0;
        uint32_t lastSuccessAtMs = 0;
        bool lastSendOk = false;
        uint32_t peerReinitCount = 0;
    };

    bool begin();

    // Fire-and-forget: increments `attempted` immediately; success/failure
    // is reflected in stats() once the async send callback fires.
    bool send(const GaugePacket &packet, uint32_t nowMs);

    // Call every loop iteration. Re-registers the peer if consecutive
    // failures cross ESPNOW_FAILURES_BEFORE_REINIT, rate-limited by
    // ESPNOW_RECOVERY_INTERVAL_MS so it never becomes a restart loop.
    void maintain(uint32_t nowMs);

    Stats stats() const;

private:
    bool registerPeer();

    uint32_t lastReinitAttemptMs_ = 0;
};
