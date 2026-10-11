#include "../../GaugePacketBuilder.h"
#include "../../SignalConfig.h"
#include "../test_framework.h"

namespace {

void testValidNormalPacket() {
    VehicleData data;
    data.oilPressurePsi = 65.0f;
    data.oilPressureValid = true;
    data.rpm = 3000;
    data.rpmValid = true;
    data.oilTemperatureC = 100.0f;
    data.oilTemperatureValid = true;
    data.engineRunning = true;

    GaugePacketBuilder builder;
    GaugePacket packet = builder.build(data, 7, 55555);

    CHECK(gaugePacketIsWellFormed(packet, sizeof(GaugePacket)));
    CHECK_EQ(packet.sequence, 7u);
    CHECK_EQ(packet.senderUptimeMs, 55555u);
    CHECK_NEAR(packet.oilPressurePsi, 65.0f, 0.001);
    CHECK_NEAR(packet.oilTemperatureF, celsiusToFahrenheit(100.0f), 0.001);
    CHECK_EQ(packet.rpm, 3000u);
    CHECK((packet.flags & FLAG_PRESSURE_VALID) != 0);
    CHECK((packet.flags & FLAG_RPM_VALID) != 0);
    CHECK((packet.flags & FLAG_OIL_TEMP_VALID) != 0);
    CHECK((packet.flags & FLAG_ENGINE_RUNNING) != 0);
    CHECK((packet.flags & FLAG_LOW_PRESSURE_WARNING) == 0);
    CHECK((packet.flags & FLAG_SENSOR_ERROR) == 0);
}

void testCriticalPressureSetsWarningFlag() {
    // GaugePacketBuilder debounces escalation (see
    // docs/can/pressure-threshold-derivation.md) -- the condition must
    // persist across calls, tracked by senderUptimeMs, before the flag
    // appears.
    VehicleData data;
    data.oilPressurePsi = 15.0f;  // well below critical (18) at this RPM
    data.oilPressureValid = true;
    data.rpm = 6500;
    data.rpmValid = true;
    data.engineRunning = true;

    GaugePacketBuilder builder;
    GaugePacket first = builder.build(data, 1, 0);
    CHECK((first.flags & FLAG_LOW_PRESSURE_WARNING) == 0);  // not yet -- just started

    GaugePacket second = builder.build(data, 2, PRESSURE_WARNING_DEBOUNCE_MS + 1);
    CHECK((second.flags & FLAG_LOW_PRESSURE_WARNING) != 0);  // persisted long enough
}

void testBriefCriticalDipDoesNotSetWarningFlag() {
    // The whole point of the debounce: a dip that recovers before
    // PRESSURE_WARNING_DEBOUNCE_MS elapses (as real track-log cornering
    // transients do) must never surface the warning flag on the wire.
    VehicleData data;
    data.oilPressurePsi = 15.0f;
    data.oilPressureValid = true;
    data.rpm = 6500;
    data.rpmValid = true;
    data.engineRunning = true;

    GaugePacketBuilder builder;
    builder.build(data, 1, 0);

    data.oilPressurePsi = 60.0f;  // recovers well before the debounce window elapses
    GaugePacket recovered = builder.build(data, 2, PRESSURE_WARNING_DEBOUNCE_MS / 2);
    CHECK((recovered.flags & FLAG_LOW_PRESSURE_WARNING) == 0);
}

void testEngineOffZeroPressureNeverWarns() {
    VehicleData data;
    data.oilPressurePsi = 0.0f;
    data.oilPressureValid = true;
    data.rpm = 0;
    data.rpmValid = true;
    data.engineRunning = false;

    GaugePacketBuilder builder;
    GaugePacket packet = builder.build(data, 1, 0);
    CHECK((packet.flags & FLAG_LOW_PRESSURE_WARNING) == 0);
}

void testSensorErrorNeverTransmitsFabricatedPressureAsValid() {
    VehicleData data;
    data.oilPressureValid = false;
    data.sensorError = true;
    data.rpm = 3000;
    data.rpmValid = true;
    data.engineRunning = true;

    GaugePacketBuilder builder;
    GaugePacket packet = builder.build(data, 1, 0);
    CHECK((packet.flags & FLAG_PRESSURE_VALID) == 0);
    CHECK((packet.flags & FLAG_SENSOR_ERROR) != 0);
    CHECK((packet.flags & FLAG_LOW_PRESSURE_WARNING) == 0);
    CHECK_NEAR(packet.oilPressurePsi, 0.0f, 0.001);  // never a stale/fabricated value on the wire
}

void testInvalidSignalsProduceZeroedButWellFormedPacket() {
    VehicleData data;  // everything invalid/default -- sender-online-no-data case
    GaugePacketBuilder builder;
    GaugePacket packet = builder.build(data, 1, 1000);

    CHECK(gaugePacketIsWellFormed(packet, sizeof(GaugePacket)));
    CHECK_EQ(packet.flags, 0u);
}

void testV2FieldsCarriedWithFlags() {
    VehicleData data;
    data.coolantTemperatureC = 90.0f;
    data.coolantTemperatureValid = true;
    data.lateralAccelerationMps2 = -9.80665f;  // exactly -1 g
    data.lateralAccelerationValid = true;

    GaugePacketBuilder builder;
    GaugePacket packet = builder.build(data, 1, 0);

    CHECK(gaugePacketIsWellFormed(packet, sizeof(GaugePacket)));
    CHECK_EQ(packet.protocolVersion, 2u);
    CHECK_NEAR(packet.coolantTemperatureF, 194.0f, 0.001);
    CHECK_NEAR(packet.lateralAccelerationG, -1.0f, 0.0001);
    CHECK((packet.flags & FLAG_COOLANT_TEMP_VALID) != 0);
    CHECK((packet.flags & FLAG_LATERAL_ACCEL_VALID) != 0);
}

void testInvalidV2FieldsZeroedAndFlagsClear() {
    VehicleData data;
    data.coolantTemperatureC = 90.0f;      // stale value left behind...
    data.lateralAccelerationMps2 = 12.0f;  // ...must never reach the wire
    GaugePacketBuilder builder;
    GaugePacket packet = builder.build(data, 1, 0);

    CHECK_NEAR(packet.coolantTemperatureF, 0.0f, 0.001);
    CHECK_NEAR(packet.lateralAccelerationG, 0.0f, 0.001);
    CHECK((packet.flags & (FLAG_COOLANT_TEMP_VALID | FLAG_LATERAL_ACCEL_VALID)) == 0);
}

void testSpeedNeverClaimedValid() {
    // No speed decoder is validated yet (docs/can/signal-map.md) -- the
    // display must keep showing N/A, never a fabricated 0 MPH.
    VehicleData data;
    data.rpm = 3000;
    data.rpmValid = true;
    data.engineRunning = true;
    GaugePacketBuilder builder;
    GaugePacket packet = builder.build(data, 1, 0);
    CHECK((packet.flags & FLAG_SPEED_VALID) == 0);
}

}  // namespace

int main() {
    testValidNormalPacket();
    testCriticalPressureSetsWarningFlag();
    testBriefCriticalDipDoesNotSetWarningFlag();
    testEngineOffZeroPressureNeverWarns();
    testSensorErrorNeverTransmitsFabricatedPressureAsValid();
    testInvalidSignalsProduceZeroedButWellFormedPacket();
    testV2FieldsCarriedWithFlags();
    testInvalidV2FieldsZeroedAndFlagsClear();
    testSpeedNeverClaimedValid();
    TEST_SUMMARY();
}
