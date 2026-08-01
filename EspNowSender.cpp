#include "EspNowSender.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <string.h>

#include "NetworkConfig.h"

namespace {

volatile uint32_t g_succeeded = 0;
volatile uint32_t g_failed = 0;
volatile uint16_t g_consecutiveFailures = 0;
volatile uint32_t g_lastSendAtMs = 0;
volatile uint32_t g_lastSuccessAtMs = 0;
volatile bool g_lastSendOk = false;
uint32_t g_attempted = 0;
uint32_t g_peerReinitCount = 0;

// Lightweight by design: only updates counters, per the project's ESP-NOW
// callback rules. No Serial, no delay, no peer/WiFi mutation, no retry.
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    (void)mac;
    uint32_t nowMs = millis();
    g_lastSendAtMs = nowMs;
    if (status == ESP_NOW_SEND_SUCCESS) {
        g_succeeded++;
        g_consecutiveFailures = 0;
        g_lastSendOk = true;
        g_lastSuccessAtMs = nowMs;
    } else {
        g_failed++;
        g_consecutiveFailures++;
        g_lastSendOk = false;
    }
}

}  // namespace

bool EspNowSender::registerPeer() {
    esp_now_del_peer(DISPLAY_MAC);  // ignore "not found" if this is the first call

    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, DISPLAY_MAC, 6);
    peer.channel = ESPNOW_CHANNEL;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;

    esp_err_t result = esp_now_add_peer(&peer);
    if (result != ESP_OK) {
        Serial.printf("[ESPNOW][ERROR] esp_now_add_peer failed: %d\n", result);
        return false;
    }
    return true;
}

bool EspNowSender::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

    Serial.printf("[ESPNOW] Sender MAC: %s\n", WiFi.macAddress().c_str());
    Serial.printf("[ESPNOW] Receiver MAC: %02X:%02X:%02X:%02X:%02X:%02X\n", DISPLAY_MAC[0], DISPLAY_MAC[1],
                  DISPLAY_MAC[2], DISPLAY_MAC[3], DISPLAY_MAC[4], DISPLAY_MAC[5]);
    Serial.printf("[ESPNOW] Channel: %u\n", ESPNOW_CHANNEL);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESPNOW][ERROR] esp_now_init failed");
        return false;
    }
    esp_now_register_send_cb(onDataSent);

    if (!registerPeer()) {
        return false;
    }
    Serial.println("[ESPNOW] Peer registered");
    return true;
}

bool EspNowSender::send(const GaugePacket &packet, uint32_t nowMs) {
    (void)nowMs;
    g_attempted++;
    esp_err_t result = esp_now_send(DISPLAY_MAC, reinterpret_cast<const uint8_t *>(&packet), sizeof(packet));
    return result == ESP_OK;
}

void EspNowSender::maintain(uint32_t nowMs) {
    if (g_consecutiveFailures < ESPNOW_FAILURES_BEFORE_REINIT) return;
    if (nowMs - lastReinitAttemptMs_ < ESPNOW_RECOVERY_INTERVAL_MS) return;

    lastReinitAttemptMs_ = nowMs;
    Serial.printf("[ESPNOW][RECOVERY] %u consecutive failures -- re-registering peer\n", g_consecutiveFailures);
    if (registerPeer()) {
        g_peerReinitCount++;
        g_consecutiveFailures = 0;
    }
}

EspNowSender::Stats EspNowSender::stats() const {
    Stats s;
    s.attempted = g_attempted;
    s.succeeded = g_succeeded;
    s.failed = g_failed;
    s.consecutiveFailures = g_consecutiveFailures;
    s.lastSendAtMs = g_lastSendAtMs;
    s.lastSuccessAtMs = g_lastSuccessAtMs;
    s.lastSendOk = g_lastSendOk;
    s.peerReinitCount = g_peerReinitCount;
    return s;
}
