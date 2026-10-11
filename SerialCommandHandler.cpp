#include "SerialCommandHandler.h"

#include <WiFi.h>

#include "SenderConfig.h"

namespace {

String nextToken(const String &s, int &cursor) {
    while (cursor < (int)s.length() && s[cursor] == ' ') cursor++;
    int start = cursor;
    while (cursor < (int)s.length() && s[cursor] != ' ') cursor++;
    return s.substring(start, cursor);
}

}  // namespace

VehicleData SerialCommandHandler::currentSourceData(SystemContext &ctx) {
    if (ctx.simulator) return ctx.simulator->data();
    if (ctx.decoder) return ctx.decoder->data();
    return VehicleData();
}

void SerialCommandHandler::poll(SystemContext &ctx, uint32_t nowMs) {
    while (Serial.available() > 0) {
        char c = static_cast<char>(Serial.read());
        if (c == '\n' || c == '\r') {
            if (lineBuffer_.length() > 0) {
                handleLine(lineBuffer_, ctx, nowMs);
                lineBuffer_ = "";
            }
        } else {
            lineBuffer_ += c;
            if (lineBuffer_.length() > 96) lineBuffer_ = "";  // guard against runaway input
        }
    }
}

void SerialCommandHandler::handleLine(const String &line, SystemContext &ctx, uint32_t nowMs) {
    String trimmed = line;
    trimmed.trim();
    if (trimmed.length() == 0) return;

    int cursor = 0;
    String cmd = nextToken(trimmed, cursor);
    String rest = trimmed.substring(cursor);
    rest.trim();
    cmd.toLowerCase();

    if (cmd == "help") {
        cmdHelp();
    } else if (cmd == "status") {
        printStatusLine(ctx, nowMs);
    } else if (cmd == "mac") {
        cmdMac();
    } else if (cmd == "can") {
        cmdCan(rest, ctx);
    } else if (cmd == "mark") {
        cmdMark(rest, nowMs);
    } else if (cmd == "espnow") {
        cmdEspNow(rest, ctx, nowMs);
    } else if (cmd == "test") {
        cmdTest(rest, ctx, nowMs);
    } else if (cmd == "simulation") {
        // Alias family for the `test ...` bench-test controls.
        String sub = rest;
        sub.toLowerCase();
        if (sub == "start") cmdTest("start", ctx, nowMs);
        else if (sub == "stop") cmdTest("stop", ctx, nowMs);
        else if (sub == "restart") cmdTest("restart", ctx, nowMs);
        else Serial.println("[CMD] usage: simulation start|stop|restart");
    } else if (cmd == "set") {
        cmdSet(rest, ctx);
    } else if (cmd == "send") {
        String sub = rest;
        sub.toLowerCase();
        if (sub == "once") cmdSendOnce(ctx, nowMs);
        else Serial.println("[CMD] usage: send once");
    } else if (cmd == "reboot") {
        Serial.println("[CMD] Rebooting...");
        delay(100);
        ESP.restart();
    } else {
        Serial.printf("[CMD] Unknown command '%s'. Type 'help'.\n", cmd.c_str());
    }
}

void SerialCommandHandler::cmdHelp() const {
    Serial.println(
        "Commands:\n"
        "  help\n"
        "  status\n"
        "  mac\n"
        "  can start | can stop | can summary | can clear\n"
        "  can filter all | can filter changed | can filter id 0xNNN\n"
        "  mark <label>\n"
        "  espnow status | espnow test\n"
        "  test status | test start | test stop | test restart | test next | test previous | test send_once\n"
        "  simulation start | simulation stop | simulation restart  (aliases of test ...)\n"
        "  set pressure|rpm|oiltemp|running|pressure_valid|rpm_valid|temp_valid|warning|sensor_error <value>\n"
        "  send once\n"
        "  reboot");
}

void SerialCommandHandler::printStatusLine(SystemContext &ctx, uint32_t nowMs) const {
    VehicleData d = currentSourceData(ctx);
    if (manualOverrideActive_) applyManualOverride(d);

    EspNowSender::Stats tx;
    if (ctx.espNowSender) tx = ctx.espNowSender->stats();

    Serial.printf(
        "[STATUS] mode=%s oil_psi=%.1f oil_valid=%d rpm=%u rpm_valid=%d oil_temp_c=%.1f oil_temp_valid=%d "
        "coolant_c=%.1f coolant_valid=%d lat_mps2=%.1f lat_valid=%d engine_running=%d sensor_error=%d espnow_tx_ok=%lu espnow_tx_fail=%lu espnow_consec_fail=%u heap=%u "
        "uptime_ms=%lu\n",
        WIRELESS_BENCH_TEST_MODE ? "BENCH_TEST" : "VEHICLE_CAN", d.oilPressurePsi, d.oilPressureValid, d.rpm,
        d.rpmValid, d.oilTemperatureC, d.oilTemperatureValid, d.coolantTemperatureC, d.coolantTemperatureValid,
        d.lateralAccelerationMps2, d.lateralAccelerationValid, d.engineRunning, d.sensorError,
        (unsigned long)tx.succeeded, (unsigned long)tx.failed, tx.consecutiveFailures,
        (unsigned)ESP.getFreeHeap(), (unsigned long)nowMs);

    if (ctx.canStats) {
        Serial.printf("[STATUS][CAN] frames=%lu fps=%.1f ids=%u\n", (unsigned long)ctx.canStats->totalFrameCount(),
                      ctx.canStats->approximateBusFps(nowMs), (unsigned)ctx.canStats->idCount());
    }
}

void SerialCommandHandler::cmdMac() const { Serial.printf("[MAC] %s\n", WiFi.macAddress().c_str()); }

void SerialCommandHandler::cmdCan(const String &args, SystemContext &ctx) const {
    if (!ctx.canLogger || !ctx.canStats || !ctx.frameHistory) {
        Serial.println("[CAN] not available (WIRELESS_BENCH_TEST_MODE has CAN disabled)");
        return;
    }

    String a = args;
    a.toLowerCase();

    if (a == "start") {
        ctx.canLogger->setMode(CanLogMode::AllFrames);
        Serial.println("[CAN] raw logging started (all frames)");
    } else if (a == "stop") {
        ctx.canLogger->setMode(CanLogMode::Disabled);
        Serial.println("[CAN] logging stopped");
    } else if (a == "summary") {
        ctx.canStats->printSummary();
    } else if (a == "clear") {
        ctx.canStats->clear();
        ctx.frameHistory->clear();
        Serial.println("[CAN] statistics and frame history cleared");
    } else if (a == "filter all") {
        ctx.canLogger->setMode(CanLogMode::AllFrames);
        Serial.println("[CAN] filter: all frames");
    } else if (a == "filter changed") {
        ctx.canLogger->setMode(CanLogMode::ChangedFrames);
        Serial.println("[CAN] filter: changed frames only");
    } else if (a.startsWith("filter id ")) {
        String idStr = a.substring(10);
        idStr.trim();
        uint32_t id = strtoul(idStr.c_str(), nullptr, 0);
        ctx.canLogger->clearIdFilter();
        ctx.canLogger->addIdFilter(id);
        ctx.canLogger->setMode(CanLogMode::SelectedIds);
        Serial.printf("[CAN] filter: id 0x%03lX only\n", (unsigned long)id);
    } else {
        Serial.println(
            "[CAN] usage: can start|stop|summary|clear|filter all|filter changed|filter id 0xNNN");
    }
}

void SerialCommandHandler::cmdMark(const String &label, uint32_t nowMs) const {
    Serial.printf("[MARK] t=%lu label=%s\n", (unsigned long)nowMs, label.length() ? label.c_str() : "(unlabeled)");
}

void SerialCommandHandler::cmdEspNow(const String &args, SystemContext &ctx, uint32_t nowMs) {
    String a = args;
    a.toLowerCase();

    if (a == "status") {
        if (!ctx.espNowSender) {
            Serial.println("[ESPNOW] not available");
            return;
        }
        EspNowSender::Stats s = ctx.espNowSender->stats();
        Serial.printf(
            "[ESPNOW] attempted=%lu ok=%lu fail=%lu consec_fail=%u last_ok=%d last_ok_age_ms=%lu reinit_count=%lu\n",
            (unsigned long)s.attempted, (unsigned long)s.succeeded, (unsigned long)s.failed, s.consecutiveFailures,
            s.lastSendOk, (unsigned long)(nowMs - s.lastSuccessAtMs), (unsigned long)s.peerReinitCount);
    } else if (a == "test") {
        cmdSendOnce(ctx, nowMs);
    } else {
        Serial.println("[ESPNOW] usage: espnow status|test");
    }
}

void SerialCommandHandler::cmdTest(const String &args, SystemContext &ctx, uint32_t nowMs) {
    if (!ctx.simulator) {
        Serial.println("[TEST] not available (build with WIRELESS_BENCH_TEST_MODE=1)");
        return;
    }

    String a = args;
    a.toLowerCase();

    if (a == "status") {
        VehicleData d = ctx.simulator->data();
        EspNowSender::Stats tx;
        if (ctx.espNowSender) tx = ctx.espNowSender->stats();
        Serial.printf(
            "[TEST] stage=%s elapsed_ms=%lu transmit=%d pressure_psi=%.1f rpm=%u oil_temp_f=%.1f running=%d "
            "sensor_error=%d sequence=%lu tx_ok=%lu tx_fail=%lu\n",
            ctx.simulator->currentStageName(), (unsigned long)ctx.simulator->stageElapsedMs(nowMs),
            ctx.simulator->shouldTransmit() && !testTransmitPaused_, d.oilPressurePsi, d.rpm,
            celsiusToFahrenheit(d.oilTemperatureC), d.engineRunning, d.sensorError,
            (unsigned long)(ctx.sequence ? *ctx.sequence : 0), (unsigned long)tx.succeeded, (unsigned long)tx.failed);
    } else if (a == "start") {
        testTransmitPaused_ = false;
        Serial.println("[TEST] transmission resumed");
    } else if (a == "stop") {
        testTransmitPaused_ = true;
        Serial.println("[TEST] transmission paused (simulator keeps running internally)");
    } else if (a == "restart") {
        ctx.simulator->restart(nowMs);
        manualOverrideActive_ = false;
        manualWarningOverrideActive_ = false;
        testTransmitPaused_ = false;
        Serial.println("[TEST] scenario restarted, manual overrides cleared");
    } else if (a == "next") {
        ctx.simulator->advanceToNextStage(nowMs);
        Serial.printf("[TEST] stage -> %s\n", ctx.simulator->currentStageName());
    } else if (a == "previous") {
        ctx.simulator->advanceToPreviousStage(nowMs);
        Serial.printf("[TEST] stage -> %s\n", ctx.simulator->currentStageName());
    } else if (a == "send_once") {
        cmdSendOnce(ctx, nowMs);
    } else {
        Serial.println("[TEST] usage: test status|start|stop|restart|next|previous|send_once");
    }
}

void SerialCommandHandler::cmdSet(const String &args, SystemContext &ctx) {
    int cursor = 0;
    String field = nextToken(args, cursor);
    String valueStr = args.substring(cursor);
    valueStr.trim();
    field.toLowerCase();

    if (field.length() == 0 || valueStr.length() == 0) {
        Serial.println(
            "[SET] usage: set pressure|rpm|oiltemp|running|pressure_valid|rpm_valid|temp_valid|warning|"
            "sensor_error <value>");
        return;
    }

    if (!manualOverrideActive_) {
        manualOverride_ = currentSourceData(ctx);
        manualOverrideActive_ = true;
    }

    float value = valueStr.toFloat();
    bool boolValue = (value != 0.0f);

    if (field == "pressure") {
        manualOverride_.oilPressurePsi = value;
    } else if (field == "rpm") {
        manualOverride_.rpm = static_cast<uint16_t>(value);
    } else if (field == "oiltemp") {
        manualOverride_.oilTemperatureC = (value - 32.0f) * 5.0f / 9.0f;  // input is Fahrenheit, per README
    } else if (field == "running") {
        manualOverride_.engineRunning = boolValue;
    } else if (field == "pressure_valid") {
        manualOverride_.oilPressureValid = boolValue;
    } else if (field == "rpm_valid") {
        manualOverride_.rpmValid = boolValue;
    } else if (field == "temp_valid") {
        manualOverride_.oilTemperatureValid = boolValue;
    } else if (field == "warning") {
        manualWarningOverrideActive_ = true;
        manualWarningOverrideValue_ = boolValue;
    } else if (field == "sensor_error") {
        manualOverride_.sensorError = boolValue;
    } else {
        Serial.printf("[SET] unknown field '%s'\n", field.c_str());
        return;
    }

    Serial.printf("[SET] %s = %s (manual override active)\n", field.c_str(), valueStr.c_str());
}

void SerialCommandHandler::cmdSendOnce(SystemContext &ctx, uint32_t nowMs) const {
    if (!ctx.espNowSender) {
        Serial.println("[SEND] ESP-NOW not available");
        return;
    }

    VehicleData d = currentSourceData(ctx);
    if (manualOverrideActive_) applyManualOverride(d);

    GaugePacketBuilder builder;
    uint32_t seq = ctx.sequence ? (*ctx.sequence)++ : 0;
    GaugePacket packet = builder.build(d, seq, nowMs);
    applyManualWarningOverride(packet);

    bool ok = ctx.espNowSender->send(packet, nowMs);
    Serial.printf("[SEND] seq=%lu pressure=%.1f rpm=%u flags=0x%04X result=%s\n", (unsigned long)packet.sequence,
                  packet.oilPressurePsi, packet.rpm, packet.flags, ok ? "QUEUED" : "FAILED");
}

void SerialCommandHandler::applyManualOverride(VehicleData &data) const {
    if (!manualOverrideActive_) return;
    data = manualOverride_;
}

void SerialCommandHandler::applyManualWarningOverride(GaugePacket &packet) const {
    if (!manualWarningOverrideActive_) return;
    if (manualWarningOverrideValue_) {
        packet.flags |= FLAG_LOW_PRESSURE_WARNING;
    } else {
        packet.flags &= ~static_cast<uint16_t>(FLAG_LOW_PRESSURE_WARNING);
    }
    gaugePacketSign(packet);  // CRC covers flags -- must re-sign after changing them
}
