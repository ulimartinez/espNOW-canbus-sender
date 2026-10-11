#pragma once

#include <stddef.h>
#include <stdint.h>

// Wireless packet contract between the glovebox ESP32 (sender, decodes CAN)
// and this display (receiver). Copy this file (and GaugePacket.cpp)
// verbatim into the sender repository so both sides agree on layout.
//
// Endianness / packing: the struct is __attribute__((packed)) and both
// sides run on ESP32 (little-endian Xtensa/RISC-V), so no byte-swapping is
// needed as long as both ends are ESP32. Do not send this struct to a
// non-ESP32 platform without adding explicit endian handling.
//
// Version history:
//   v1 (26 bytes): pressure, oil temp, RPM, flags.
//   v2 (38 bytes): v1 fields unchanged and in the same position, plus
//                  coolant temp, vehicle speed, lateral acceleration
//                  appended before the checksum, plus three validity flags.
// The sender only ever transmits the current version. The display accepts
// both, so a display updated first keeps working with an un-updated sender
// (the v2-only fields simply read as invalid). An un-updated display
// rejects v2 packets as malformed -- update the display first.

constexpr uint16_t GAUGE_PACKET_MAGIC = 0x4752;  // "GR"
constexpr uint8_t GAUGE_PROTOCOL_VERSION = 2;
constexpr uint8_t GAUGE_PROTOCOL_VERSION_V1 = 1;

enum GaugePacketFlags : uint16_t {
    FLAG_PRESSURE_VALID = 1 << 0,
    FLAG_RPM_VALID = 1 << 1,
    FLAG_OIL_TEMP_VALID = 1 << 2,
    FLAG_LOW_PRESSURE_WARNING = 1 << 3,
    FLAG_ENGINE_RUNNING = 1 << 4,
    FLAG_SENSOR_ERROR = 1 << 5,
    // v2+
    FLAG_COOLANT_TEMP_VALID = 1 << 6,
    FLAG_SPEED_VALID = 1 << 7,
    FLAG_LATERAL_ACCEL_VALID = 1 << 8
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

    // --- v2 additions ---
    float coolantTemperatureF;
    float vehicleSpeedMph;
    // Signed, in g. Sign convention is the car's own (0x13B as decoded);
    // which direction is positive has not been verified, so consumers
    // should treat the magnitude as the meaningful value.
    float lateralAccelerationG;

    uint16_t checksum;  // CRC-16/CCITT-FALSE over every byte before this field
};

static_assert(sizeof(GaugePacket) == 38, "GaugePacket layout changed — update GAUGE_PROTOCOL_VERSION");

// Frozen v1 layout, kept only so the receiver can still accept packets from
// a sender that hasn't been updated yet. Never transmitted by current code.
struct __attribute__((packed)) GaugePacketV1 {
    uint16_t magic;
    uint8_t protocolVersion;
    uint8_t packetSize;

    uint32_t sequence;
    uint32_t senderUptimeMs;

    float oilPressurePsi;
    float oilTemperatureF;

    uint16_t rpm;
    uint16_t flags;

    uint16_t checksum;
};

static_assert(sizeof(GaugePacketV1) == 26, "GaugePacketV1 is a frozen wire format — never change it");

// CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over `len` bytes.
uint16_t gaugePacketCrc16(const uint8_t *data, size_t len);

// Fills packet.checksum from the rest of the packet's contents.
void gaugePacketSign(GaugePacket &packet);

// Structural + integrity checks only (magic/version/size/checksum). Does
// NOT check value plausibility — see GaugeConfig.h limits for that, applied
// by the receiver after this passes.
bool gaugePacketIsWellFormed(const GaugePacket &packet, size_t receivedLen);

// Same checks against the frozen v1 layout.
bool gaugePacketV1IsWellFormed(const GaugePacketV1 &packet, size_t receivedLen);

// Widens a well-formed v1 packet into the current layout. Every v2-only
// field is zeroed and its validity flag is clear, so nothing downstream
// can mistake "the old sender doesn't send this" for a real reading.
GaugePacket gaugePacketFromV1(const GaugePacketV1 &v1);
