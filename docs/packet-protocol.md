# Packet protocol

The wire protocol is defined by the **display repository** (`../espNOW-car-gauge/GaugePacket.h`) and reused verbatim here -- this repo does not define a second packet structure. See that repo's own documentation for the authoritative description; this page summarizes what the sender must do to stay compatible.

## Layout (protocol v2, 38 bytes, packed)

```cpp
struct __attribute__((packed)) GaugePacket {
    uint16_t magic;             // GAUGE_PACKET_MAGIC = 0x4752 ("GR")
    uint8_t protocolVersion;    // GAUGE_PROTOCOL_VERSION = 2
    uint8_t packetSize;         // sizeof(GaugePacket), always 38

    uint32_t sequence;
    uint32_t senderUptimeMs;

    float oilPressurePsi;
    float oilTemperatureF;      // Fahrenheit on the wire -- see unit conversion below

    uint16_t rpm;
    uint16_t flags;             // GaugePacketFlags bitfield

    // --- v2 additions (appended; v1 field offsets unchanged) ---
    float coolantTemperatureF;
    float vehicleSpeedMph;      // always 0 / FLAG_SPEED_VALID clear for now
    float lateralAccelerationG; // signed; sign convention unverified

    uint16_t checksum;          // CRC-16/CCITT-FALSE over every prior byte
};
```

Both ends are ESP32 (little-endian); no explicit endian handling is done or needed.

## Version compatibility

| Sender | Display | Result |
| --- | --- | --- |
| v2 | v2 | Everything works. |
| v1 (old) | v2 | Works. The display accepts the 26-byte v1 layout too (`GaugePacketV1`, `gaugePacketFromV1()`); coolant/speed/lateral read as invalid, so Warm-Up shows coolant `--` and Drive Stats show `N/A`. |
| v2 | v1 (old) | **No data.** An old display rejects 38-byte packets as malformed (safe failure: `NO DATA`, never wrong values). |

**Update the display first**, then the sender.

## Flags

```cpp
FLAG_PRESSURE_VALID         = 1 << 0
FLAG_RPM_VALID              = 1 << 1
FLAG_OIL_TEMP_VALID         = 1 << 2
FLAG_LOW_PRESSURE_WARNING   = 1 << 3
FLAG_ENGINE_RUNNING         = 1 << 4
FLAG_SENSOR_ERROR           = 1 << 5
FLAG_COOLANT_TEMP_VALID     = 1 << 6   // v2
FLAG_SPEED_VALID            = 1 << 7   // v2 -- never set until a speed decoder is validated
FLAG_LATERAL_ACCEL_VALID    = 1 << 8   // v2
```

`GaugePacketBuilder::build()` is the single place that sets these from `VehicleData` + the locally-evaluated `PressureStatus` -- see [`GaugePacketBuilder.cpp`](../GaugePacketBuilder.cpp). A field whose validity flag is clear is always transmitted as `0`.

## Unit conversion boundary

`VehicleData` stores temperatures in Celsius (matching how the CAN signal is decoded: `raw - 40`) and lateral acceleration in m/s². The wire format wants Fahrenheit and g. `GaugePacketBuilder` is the *only* place these conversions happen:

```cpp
packet.oilTemperatureF = data.oilTemperatureValid ? celsiusToFahrenheit(data.oilTemperatureC) : 0.0f;
packet.coolantTemperatureF = data.coolantTemperatureValid ? celsiusToFahrenheit(data.coolantTemperatureC) : 0.0f;
packet.lateralAccelerationG = data.lateralAccelerationValid ? data.lateralAccelerationMps2 / STANDARD_GRAVITY_MPS2 : 0.0f;
```

Oil pressure is already PSI on both sides of the boundary -- no conversion needed (this specifically differs from the RaceChrono profile's own `h / 0.145` PSI->kPa conversion, which this firmware does **not** apply; see [ansix-oil-pressure.md](can/ansix-oil-pressure.md)).

## Fields not on the wire

Accelerator position, steering angle, and brake pressure are decoded and available in `VehicleData` (and printed in diagnostics), but are not transmitted. Adding a field requires bumping `GAUGE_PROTOCOL_VERSION`, updating `GaugePacket.h`/`.cpp` **in both repositories** identically, and keeping the display able to decode the previous version.

## Sequence numbers

`main.ino` owns a single `uint32_t gSequence`, incremented once per **attempted** transmission (whether or not the send succeeds), reset only on reboot, allowed to roll over naturally. This lets the receiver detect drops, duplicates, sender restarts, and out-of-order delivery.

## Checksum

`gaugePacketSign()` (in `GaugePacket.cpp`, from the display repo) fills `magic`/`protocolVersion`/`packetSize`/`checksum` from the rest of the struct. Anything that mutates `flags` or another field after building the packet -- e.g. `SerialCommandHandler`'s manual warning-flag override for bench testing -- **must call `gaugePacketSign()` again**, since the CRC covers those bytes.
