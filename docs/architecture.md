# Architecture

## Scope

This repository is the **glovebox CAN-reading / ESP-NOW-sending firmware only**. It never renders anything -- the display firmware lives in the sibling `espNOW-car-gauge` repository, which is authoritative for the wireless packet format (see [packet-protocol.md](packet-protocol.md)).

## Pipeline

```text
Ansix oil-pressure sensor
      |
Ansix CAN module
      |
Vehicle CAN network (500 kbit/s)
      |
CANH / CANL
      |
Adafruit CAN PAL (TJA1051/T3) transceiver
      |
ESP32 TWAI controller (listen-only)          <-- CanReceiver.h/.cpp
      |
CanFrame (host-testable mirror of twai_message_t)
      |
Signal decoders                              <-- SignalDecoders.h/.cpp
      |
VehicleCanDecoder                             <-- freshness, engine-running hysteresis
      |
VehicleData                                   <-- central decoded-signal state
      |
PressureWarningLogic                          <-- supplemental RPM-interpolated thresholds
      |
GaugePacketBuilder                            <-- unit conversion at the wire boundary only
      |
GaugePacket (protocol reused verbatim from the display repo)
      |
EspNowSender                                  <-- peer registration, send, recovery
      |
ESP-NOW (unicast, channel must match display)
      |
Elecrow DHE38128D display (espNOW-car-gauge, separate repo)
```

`VehicleSimulator` is a second data source that feeds the exact same `VehicleData` -> `PressureWarningLogic` -> `GaugePacketBuilder` -> `EspNowSender` pipeline, selected instead of `CanReceiver`/`VehicleCanDecoder` at compile time via `WIRELESS_BENCH_TEST_MODE` in [`SenderConfig.h`](../SenderConfig.h). This is what makes the wireless bench test a validation of the *production* code path rather than a separate toy path -- see [validation-plan.md](validation-plan.md).

## Why the source tree is flat

Arduino's classic build system (confirmed empirically with `arduino-cli` 1.5.1 / `esp32:esp32` core 2.0.17 against this project) only compiles `.cpp` files that sit directly in the sketch root -- it does not descend into subdirectories, including a top-level `src/` folder, when they're part of the main sketch (as opposed to a library). The display repository (`espNOW-car-gauge`) hit the same limitation and documented it. Rather than fight the toolchain, all sketch source in this repo is flat at the repository root; `docs/`, `scripts/`, and `tests/` (none of which the Arduino builder touches) keep their natural subdirectory structure.

## Module responsibilities

| File | Responsibility |
| --- | --- |
| `BoardConfig.h` | GPIO pin assignments, serial baud rate |
| `SenderConfig.h` | Mode flags (`WIRELESS_BENCH_TEST_MODE`, `CAN_ENABLED`, etc.), firmware version |
| `NetworkConfig.h` | Display MAC, ESP-NOW channel, recovery tuning |
| `CanConfig.h` | TWAI bitrate/queue sizing, important CAN IDs |
| `SignalConfig.h` | Per-signal plausibility limits and freshness timeouts |
| `SignalDecoders.h/.cpp` | Pure per-signal decode formulas (host-testable) |
| `VehicleData.h` | Central decoded-signal state struct |
| `VehicleCanDecoder.h/.cpp` | Frame dispatch, plausibility, freshness, engine-running hysteresis (host-testable) |
| `PressureWarningLogic.h/.cpp` | RPM-interpolated, track-log-derived, debounced supplemental warning thresholds (host-testable) |
| `GaugePacket.h/.cpp` | Wire protocol, copied verbatim from the display repo |
| `GaugePacketBuilder.h/.cpp` | `VehicleData` -> `GaugePacket`, the only unit-conversion boundary (host-testable) |
| `CanReceiver.h/.cpp` | TWAI init/read/alerts, listen-only only |
| `CanStatistics.h/.cpp` | Per-ID counters, approximate bus fps |
| `FrameHistory.h/.cpp` | Ring buffer of recent frames for the decoded IDs |
| `CanLogger.h/.cpp` | Raw/changed/filtered candump-style Serial logging |
| `EspNowSender.h/.cpp` | ESP-NOW init, peer registration, lightweight send callback, recovery |
| `VehicleSimulator.h/.cpp` | Wireless bench-test scenario generator |
| `SerialCommandHandler.h/.cpp` | Serial command parsing, manual override test mode |
| `espNOW-canbus-sender.ino` | `setup()`/`loop()` wiring |
