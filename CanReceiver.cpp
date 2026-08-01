#include "CanReceiver.h"

#include <Arduino.h>

#include "BoardConfig.h"
#include "CanConfig.h"

namespace {
constexpr uint32_t ALERTS_TO_ENABLE = TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR |
                                       TWAI_ALERT_BUS_OFF | TWAI_ALERT_RECOVERY_IN_PROGRESS |
                                       TWAI_ALERT_BUS_RECOVERED;
constexpr uint32_t RECOVERY_LOG_THROTTLE_MS = 2000;
}  // namespace

bool CanReceiver::begin() {
    twai_general_config_t generalConfig = TWAI_GENERAL_CONFIG_DEFAULT(CAN_TX_GPIO, CAN_RX_GPIO, TWAI_MODE_LISTEN_ONLY);
    generalConfig.rx_queue_len = CAN_RX_QUEUE_LEN;

    twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&generalConfig, &timingConfig, &filterConfig) != ESP_OK) {
        Serial.println("[CAN][ERROR] twai_driver_install failed");
        return false;
    }

    if (twai_start() != ESP_OK) {
        Serial.println("[CAN][ERROR] twai_start failed");
        return false;
    }

    twai_reconfigure_alerts(ALERTS_TO_ENABLE, nullptr);

    Serial.printf("[CAN] TWAI initialized: %lu bps, LISTEN-ONLY, TX_GPIO=%d RX_GPIO=%d\n",
                  (unsigned long)CAN_BITRATE_BPS, (int)CAN_TX_GPIO, (int)CAN_RX_GPIO);
    return true;
}

bool CanReceiver::readFrame(twai_message_t &frame) { return twai_receive(&frame, 0) == ESP_OK; }

void CanReceiver::handleAlerts(uint32_t nowMs) {
    uint32_t alerts = 0;
    if (twai_read_alerts(&alerts, 0) != ESP_OK) return;

    if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
        health_.rxQueueOverflowCount++;
    }
    if (alerts & TWAI_ALERT_ERR_PASS) {
        health_.errorPassiveCount++;
    }
    if (alerts & TWAI_ALERT_BUS_OFF) {
        health_.busOffCount++;
        health_.busOff = true;
        // Controlled recovery only -- this keeps the controller in
        // listen-only mode; it is never reconfigured to be transmit-capable.
        twai_initiate_recovery();
    }
    if (alerts & TWAI_ALERT_BUS_RECOVERED) {
        health_.busRecoveredCount++;
        health_.busOff = false;
        twai_start();
    }

    if ((alerts & (TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_OFF | TWAI_ALERT_BUS_RECOVERED)) &&
        (nowMs - lastRecoveryLogMs_ > RECOVERY_LOG_THROTTLE_MS)) {
        lastRecoveryLogMs_ = nowMs;
        Serial.printf("[CAN][ALERT] overflow=%lu errPassive=%lu busOff=%lu recovered=%lu\n",
                      (unsigned long)health_.rxQueueOverflowCount, (unsigned long)health_.errorPassiveCount,
                      (unsigned long)health_.busOffCount, (unsigned long)health_.busRecoveredCount);
    }
}
