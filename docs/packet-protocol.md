# Packet protocol

The wire protocol is defined by the **display repository** (`../espNOW-car-gauge/GaugePacket.h`) and reused verbatim here -- this repo does not define a second packet structure. See that repo's own documentation for the authoritative description; this page summarizes what the sender must do to stay compatible.

## Layout (26 bytes, packed)

```cpp
struct __attribute__((packed)) GaugePacket {
    uint16_t magic;             // GAUGE_PACKET_MAGIC = 0x4752 ("GR")
    uint8_t protocolVersion;    // GAUGE_PROTOCOL_VERSION = 1
    uint8_t packetSize;         // sizeof(GaugePacket), always 26

    uint32_t sequence;
    uint32_t senderUptimeMs;

    float oilPressurePsi;
    float oilTemperatureF;      // Fahrenheit on the wire -- see unit conversion below

    uint16_t rpm;
    uint16_t flags;             // GaugePacketFlags bitfield

    uint16_t checksum;          // CRC-16/CCITT-FALSE over every prior byte
};
```

Both ends are ESP32 (little-endian); no explicit endian handling is done or needed.

## Flags

```cpp
FLAG_PRESSURE_VALID        = 1 << 0
FLAG_RPM_VALID              = 1 << 1
FLAG_OIL_TEMP_VALID         = 1 << 2
FLAG_LOW_PRESSURE_WARNING   = 1 << 3
FLAG_ENGINE_RUNNING         = 1 << 4
FLAG_SENSOR_ERROR           = 1 << 5
```

`GaugePacketBuilder::build()` is the single place that sets these from `VehicleData` + the locally-evaluated `PressureStatus` -- see [`GaugePacketBuilder.cpp`](../GaugePacketBuilder.cpp).

## Unit conversion boundary

`VehicleData` stores temperature in Celsius (matching how the CAN signal is decoded: `raw - 40`). The wire format wants Fahrenheit. `GaugePacketBuilder` is the *only* place this conversion happens:

```cpp
packet.oilTemperatureF = data.oilTemperatureValid ? celsiusToFahrenheit(data.oilTemperatureC) : 0.0f;
```

Oil pressure is already PSI on both sides of the boundary -- no conversion needed (this specifically differs from the RaceChrono profile's own `h / 0.145` PSI->kPa conversion, which this firmware does **not** apply; see [ansix-oil-pressure.md](can/ansix-oil-pressure.md)).

## Fields not on the wire (yet)

The deployed packet does not carry coolant temperature, accelerator position, steering angle, brake pressure, or lateral acceleration. All of these are decoded and available in `VehicleData` (and printed in diagnostics), but are intentionally not transmitted -- adding them requires bumping `GAUGE_PROTOCOL_VERSION`, updating `GaugePacket.h` **in both repositories** identically, and defining backward-compatibility behavior for a receiver still running the old version. Do not extend the packet unilaterally from this repo.

## Sequence numbers

`main.ino` owns a single `uint32_t gSequence`, incremented once per **attempted** transmission (whether or not the send succeeds), reset only on reboot, allowed to roll over naturally. This lets the receiver detect drops, duplicates, sender restarts, and out-of-order delivery.

## Checksum

`gaugePacketSign()` (in `GaugePacket.cpp`, from the display repo) fills `magic`/`protocolVersion`/`packetSize`/`checksum` from the rest of the struct. Anything that mutates `flags` or another field after building the packet -- e.g. `SerialCommandHandler`'s manual warning-flag override for bench testing -- **must call `gaugePacketSign()` again**, since the CRC covers those bytes.
