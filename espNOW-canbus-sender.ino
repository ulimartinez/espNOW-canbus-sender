#include <WiFi.h>

#include "BoardConfig.h"
#include "CanConfig.h"
#include "GaugePacketBuilder.h"
#include "NetworkConfig.h"
#include "SenderConfig.h"
#include "SerialCommandHandler.h"

#if WIRELESS_BENCH_TEST_MODE
#include "VehicleSimulator.h"
VehicleSimulator gSimulator;
#else
#include "CanLogger.h"
#include "CanReceiver.h"
#include "CanStatistics.h"
#include "FrameHistory.h"
#include "VehicleCanDecoder.h"
CanReceiver gCanReceiver;
VehicleCanDecoder gDecoder;
CanStatistics gCanStats;
CanLogger gCanLogger;
FrameHistory gFrameHistory;
#endif

EspNowSender gEspNowSender;
GaugePacketBuilder gPacketBuilder;
SerialCommandHandler gSerialHandler;

uint32_t gSequence = 0;

namespace {

SystemContext buildContext() {
    SystemContext ctx;
    ctx.sequence = &gSequence;
    ctx.espNowSender = &gEspNowSender;
#if WIRELESS_BENCH_TEST_MODE
    ctx.simulator = &gSimulator;
#else
    ctx.decoder = &gDecoder;
    ctx.canReceiver = &gCanReceiver;
    ctx.canStats = &gCanStats;
    ctx.canLogger = &gCanLogger;
    ctx.frameHistory = &gFrameHistory;
#endif
    return ctx;
}

}  // namespace

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    delay(300);

    Serial.println("--- GR86/BRZ CAN -> ESP-NOW sender booting ---");
    Serial.printf("[BOOT] %s v%s (protocol v%u, git %s, built %s %s)\n", FIRMWARE_NAME, FIRMWARE_VERSION,
                  GAUGE_PROTOCOL_VERSION, FIRMWARE_BUILD_GIT_HASH, __DATE__, __TIME__);

#if WIRELESS_BENCH_TEST_MODE
    Serial.println("[MODE] WIRELESS BENCH TEST");
    Serial.println("[MODE] Vehicle CAN is disabled");
    Serial.println("[MODE] Transmitting simulated gauge data");
#else
    Serial.println("[MODE] VEHICLE CAN");
    Serial.println("[MODE] Simulation is disabled");
#endif

    WiFi.mode(WIFI_STA);
    Serial.printf("[BOOT] Local MAC: %s\n", WiFi.macAddress().c_str());

#if ESPNOW_ENABLED
    if (!gEspNowSender.begin()) {
        Serial.println("[BOOT][ERROR] ESP-NOW init failed -- will keep retrying via maintain()");
    }
#endif

#if WIRELESS_BENCH_TEST_MODE
    gSimulator.begin(millis());
#else
    if (!gCanReceiver.begin()) {
        Serial.println("[BOOT][ERROR] TWAI init failed -- CAN data will not be available");
    }
#endif

    Serial.println("[BOOT] Setup complete -- entering data-waiting state");
}

void loop() {
    uint32_t nowMs = millis();

#if WIRELESS_BENCH_TEST_MODE
    gSimulator.update(nowMs);
#else
    twai_message_t rawFrame;
    for (uint16_t i = 0; i < CAN_MAX_FRAMES_PER_LOOP; i++) {
        if (!gCanReceiver.readFrame(rawFrame)) break;
        CanFrame frame = twaiMessageToCanFrame(rawFrame);
        gCanStats.recordFrame(frame, nowMs);
        gFrameHistory.record(frame, nowMs);
#if CAN_RAW_LOG_ENABLED
        gCanLogger.maybeLog(frame, nowMs);
#endif
        gDecoder.processFrame(frame, nowMs);
    }
    gCanReceiver.handleAlerts(nowMs);
    gDecoder.updateFreshness(nowMs);
#endif

    SystemContext ctx = buildContext();
    gSerialHandler.poll(ctx, nowMs);

#if ESPNOW_ENABLED
    static uint32_t lastSendMs = 0;
    if (nowMs - lastSendMs >= ESPNOW_SEND_INTERVAL_MS) {
        lastSendMs = nowMs;

        bool shouldSend = !gSerialHandler.transmitPaused();
#if WIRELESS_BENCH_TEST_MODE
        shouldSend = shouldSend && gSimulator.shouldTransmit();
#endif

        if (shouldSend) {
#if WIRELESS_BENCH_TEST_MODE
            VehicleData data = gSimulator.data();
#else
            VehicleData data = gDecoder.data();
#endif
            gSerialHandler.applyManualOverride(data);

            uint32_t seq = gSequence++;
            GaugePacket packet = gPacketBuilder.build(data, seq, nowMs);
            gSerialHandler.applyManualWarningOverride(packet);
            gEspNowSender.send(packet, nowMs);

#if VERBOSE_LOGGING_ENABLED
            static uint32_t lastTxLogMs = 0;
            if (nowMs - lastTxLogMs >= TEST_LOG_INTERVAL_MS) {
                lastTxLogMs = nowMs;
                Serial.printf("[TX] seq=%lu pressure=%.1f rpm=%u oil_temp_f=%.1f flags=0x%04X crc=0x%04X\n",
                              (unsigned long)packet.sequence, packet.oilPressurePsi, packet.rpm,
                              packet.oilTemperatureF, packet.flags, packet.checksum);
            }
#endif
        }
    }

    gEspNowSender.maintain(nowMs);
#endif

    static uint32_t lastStatusMs = 0;
    if (nowMs - lastStatusMs >= SERIAL_STATUS_INTERVAL_MS) {
        lastStatusMs = nowMs;
        gSerialHandler.printStatusLine(ctx, nowMs);
    }

    delay(2);
}
