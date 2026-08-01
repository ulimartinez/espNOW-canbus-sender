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

}  // namespace

int main() {
    testCrcKnownVector();
    testSignAndValidateRoundTrip();
    testCorruptionDetected();
    testNonFinitePayloadRejected();
    TEST_SUMMARY();
}
