#pragma once

#include <stddef.h>
#include <stdint.h>

// Wireless packet contract between the glovebox ESP32 (sender, decodes CAN)
// and this display (receiver). Copy this file verbatim into the sender
// repository so both sides agree on layout.
//
// Endianness / packing: the struct is __attribute__((packed)) and both
// sides run on ESP32 (little-endian Xtensa/RISC-V), so no byte-swapping is
// needed as long as both ends are ESP32. Do not send this struct to a
// non-ESP32 platform without adding explicit endian handling.

constexpr uint16_t GAUGE_PACKET_MAGIC = 0x4752;  // "GR"
constexpr uint8_t GAUGE_PROTOCOL_VERSION = 1;

enum GaugePacketFlags : uint16_t {
    FLAG_PRESSURE_VALID = 1 << 0,
    FLAG_RPM_VALID = 1 << 1,
    FLAG_OIL_TEMP_VALID = 1 << 2,
    FLAG_LOW_PRESSURE_WARNING = 1 << 3,
    FLAG_ENGINE_RUNNING = 1 << 4,
    FLAG_SENSOR_ERROR = 1 << 5
};

struct __attribute__((packed)) GaugePacket {
    uint16_t magic;
    uint8_t protocolVersion;
    uint8_t packetSize;

    uint32_t sequence;
    uint32_t senderUptimeMs;

    float oilPressurePsi;
    float oilTemperatureF;

    uint16_t rpm;
    uint16_t flags;

    uint16_t checksum;  // CRC-16/CCITT-FALSE over every byte before this field
};

static_assert(sizeof(GaugePacket) == 26, "GaugePacket layout changed — update GAUGE_PROTOCOL_VERSION");

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over `len` bytes.
uint16_t gaugePacketCrc16(const uint8_t *data, size_t len);

// Fills packet.checksum from the rest of the packet's contents.
void gaugePacketSign(GaugePacket &packet);

// Structural + integrity checks only (magic/version/size/checksum). Does
// NOT check value plausibility — see GaugeConfig.h limits for that, applied
// by the receiver after this passes.
bool gaugePacketIsWellFormed(const GaugePacket &packet, size_t receivedLen);
