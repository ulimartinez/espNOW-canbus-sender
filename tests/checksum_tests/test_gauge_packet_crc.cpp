#include <cstddef>
#include <cstring>

#include "../../GaugePacket.h"
#include "../test_framework.h"

namespace {

void testCrcKnownVector() {
    // Standard CRC-16/CCITT-FALSE check value (poly 0x1021, init 0xFFFF,
    // no reflect, xorout 0) for ASCII "123456789" is 0x29B1.
    const uint8_t input[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK_EQ(gaugePacketCrc16(input, sizeof(input)), 0x29B1);
}

void testSignAndValidateRoundTrip() {
    GaugePacket packet{};
    packet.sequence = 42;
    packet.senderUptimeMs = 12345;
    packet.oilPressurePsi = 72.5f;
    packet.oilTemperatureF = 224.0f;
    packet.rpm = 6500;
    packet.flags = FLAG_PRESSURE_VALID | FLAG_RPM_VALID | FLAG_ENGINE_RUNNING;

    gaugePacketSign(packet);

    CHECK_EQ(packet.magic, GAUGE_PACKET_MAGIC);
    CHECK_EQ(packet.protocolVersion, GAUGE_PROTOCOL_VERSION);
    CHECK_EQ(packet.packetSize, sizeof(GaugePacket));
    CHECK(gaugePacketIsWellFormed(packet, sizeof(GaugePacket)));
}

void testCorruptionDetected() {
    GaugePacket packet{};
    packet.oilPressurePsi = 30.0f;
    packet.oilTemperatureF = 200.0f;
    packet.rpm = 900;
    packet.flags = FLAG_PRESSURE_VALID;
    gaugePacketSign(packet);

    GaugePacket corrupted = packet;
    corrupted.oilPressurePsi = 999.0f;  // change payload without re-signing
    CHECK(!gaugePacketIsWellFormed(corrupted, sizeof(GaugePacket)));

    GaugePacket wrongSize = packet;
    CHECK(!gaugePacketIsWellFormed(wrongSize, sizeof(GaugePacket) - 1));

    GaugePacket wrongMagic = packet;
    wrongMagic.magic = 0x0000;
    CHECK(!gaugePacketIsWellFormed(wrongMagic, sizeof(GaugePacket)));

    GaugePacket wrongVersion = packet;
    wrongVersion.protocolVersion = GAUGE_PROTOCOL_VERSION + 1;
    CHECK(!gaugePacketIsWellFormed(wrongVersion, sizeof(GaugePacket)));
}

void testNonFinitePayloadRejected() {
    GaugePacket packet{};
    packet.oilPressurePsi = 0.0f / 0.0f;  // NaN
    packet.oilTemperatureF = 200.0f;
    gaugePacketSign(packet);
    CHECK(!gaugePacketIsWellFormed(packet, sizeof(GaugePacket)));
}

void testV1PacketStillAcceptedAndWidened() {
    // A display running this code must keep accepting an un-updated (v1)
    // sender, with the v2-only fields reading as invalid.
    GaugePacketV1 v1{};
    v1.magic = GAUGE_PACKET_MAGIC;
    v1.protocolVersion = GAUGE_PROTOCOL_VERSION_V1;
    v1.packetSize = sizeof(GaugePacketV1);
    v1.sequence = 9;
    v1.oilPressurePsi = 41.0f;
    v1.oilTemperatureF = 205.0f;
    v1.rpm = 2500;
    v1.flags = FLAG_PRESSURE_VALID | FLAG_RPM_VALID | FLAG_OIL_TEMP_VALID | FLAG_ENGINE_RUNNING;
    v1.checksum = gaugePacketCrc16(reinterpret_cast<const uint8_t *>(&v1), offsetof(GaugePacketV1, checksum));

    CHECK(gaugePacketV1IsWellFormed(v1, sizeof(GaugePacketV1)));
    CHECK(!gaugePacketV1IsWellFormed(v1, sizeof(GaugePacket)));

    GaugePacket wide = gaugePacketFromV1(v1);
    CHECK_EQ(wide.sequence, 9u);
    CHECK_NEAR(wide.oilPressurePsi, 41.0f, 0.001);
    CHECK_EQ(wide.rpm, 2500u);
    CHECK((wide.flags & (FLAG_COOLANT_TEMP_VALID | FLAG_SPEED_VALID | FLAG_LATERAL_ACCEL_VALID)) == 0);

    GaugePacketV1 corrupted = v1;
    corrupted.rpm = 7000;
    CHECK(!gaugePacketV1IsWellFormed(corrupted, sizeof(GaugePacketV1)));
}

void testV2NonFiniteNewFieldsRejected() {
    GaugePacket packet{};
    packet.lateralAccelerationG = 0.0f / 0.0f;
    gaugePacketSign(packet);
    CHECK(!gaugePacketIsWellFormed(packet, sizeof(GaugePacket)));
}

}  // namespace

int main() {
    testCrcKnownVector();
    testSignAndValidateRoundTrip();
    testCorruptionDetected();
    testNonFinitePayloadRejected();
    testV1PacketStillAcceptedAndWidened();
    testV2NonFiniteNewFieldsRejected();
    TEST_SUMMARY();
}
