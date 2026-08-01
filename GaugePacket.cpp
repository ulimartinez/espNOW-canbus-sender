#include "GaugePacket.h"

#include <math.h>
#include <string.h>

uint16_t gaugePacketCrc16(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

void gaugePacketSign(GaugePacket &packet) {
    packet.magic = GAUGE_PACKET_MAGIC;
    packet.protocolVersion = GAUGE_PROTOCOL_VERSION;
    packet.packetSize = sizeof(GaugePacket);
    packet.checksum = gaugePacketCrc16(
        reinterpret_cast<const uint8_t *>(&packet),
        offsetof(GaugePacket, checksum));
}

bool gaugePacketIsWellFormed(const GaugePacket &packet, size_t receivedLen) {
    if (receivedLen != sizeof(GaugePacket)) return false;
    if (packet.magic != GAUGE_PACKET_MAGIC) return false;
    if (packet.protocolVersion != GAUGE_PROTOCOL_VERSION) return false;
    if (packet.packetSize != sizeof(GaugePacket)) return false;

    uint16_t expected = gaugePacketCrc16(
        reinterpret_cast<const uint8_t *>(&packet),
        offsetof(GaugePacket, checksum));
    if (packet.checksum != expected) return false;

    if (!isfinite(packet.oilPressurePsi)) return false;
    if (!isfinite(packet.oilTemperatureF)) return false;

    return true;
}
