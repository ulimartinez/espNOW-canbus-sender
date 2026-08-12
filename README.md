# espNOW-canbus-sender

Glovebox-side firmware for a 2022+ Toyota GR86 / Subaru BRZ: passively reads the vehicle CAN bus, decodes oil pressure (and several supplemental signals), and transmits normalized gauge data over ESP-NOW to a separate display.

## 1. Purpose

Reads vehicle CAN traffic, decodes an Ansix oil-pressure CAN module plus several signals from a supplied RaceChrono configuration, tracks each signal's validity/freshness independently, and sends a compact status packet over ESP-NOW at 20-50 Hz to a dashboard-mounted display. This ESP32 never renders anything and never talks back to the vehicle.

## 2. Supported vehicle

2022+ Toyota GR86 / Subaru BRZ, with an Ansix 2022+ BRZ/GR86 Oil Pressure Sensor CANbus Kit installed. CAN signal mappings are sourced from the Ansix AiM configuration and a supplied RaceChrono `.rcz` vehicle profile -- see [`docs/can/`](docs/can/).

## 3. Hardware list

- ESP-WROOM-32 dev board (original ESP32, 3.3V logic, built-in TWAI controller, Wi-Fi for ESP-NOW)
- Adafruit CAN PAL (TJA1051/T3 transceiver, 3-5V capable, switchable termination resistor)
- Ansix 2022+ BRZ/GR86 Oil Pressure Sensor CANbus Kit (vehicle-side, broadcasts on the CAN bus this firmware taps, via the Ansix/ASC connector -- see [`docs/wiring.md`](docs/wiring.md))
- Automotive 12V-to-5V converter or USB power source for the ESP32 (never bare vehicle 12V)
- Elecrow DHE38128D display, running the separate `espNOW-car-gauge` firmware (not part of this repo)

## 4. System architecture

```text
Ansix oil-pressure sensor -> Ansix CAN module -> Vehicle CAN network -> CANH/CANL
   -> Adafruit CAN PAL -> ESP32 TWAI (listen-only) -> signal decoders
   -> VehicleData -> warning logic -> GaugePacket -> ESP-NOW -> Elecrow DHE38128D display
```

See [`docs/architecture.md`](docs/architecture.md) for the full module-by-module breakdown, including why the sketch source is intentionally flat (Arduino's classic builder doesn't compile `.cpp` files in sketch subdirectories -- confirmed empirically against this toolchain).

## 5. Repository scope

This repo is the **glovebox CAN-reading / ESP-NOW-sending firmware only**. It never renders a display, never joins a Wi-Fi network (ESP-NOW runs directly on a fixed channel), and never transmits on the vehicle CAN bus.

## 6. Relationship to the display repository

The sibling repository `../espNOW-car-gauge` is the **display firmware** and is authoritative for the wireless packet format, ESP-NOW channel, sender-authorization behavior, receiver timeout behavior, and expected units. `GaugePacket.h`/`GaugePacket.cpp` in this repo are copied verbatim from that repository -- do not modify them here without updating both repos and bumping the protocol version. See [`docs/packet-protocol.md`](docs/packet-protocol.md).

## 7. Wiring diagram

See [`docs/wiring.md`](docs/wiring.md) for the full pinout, and read it in full before connecting anything to a vehicle.

```text
ESP32          CAN PAL              Ansix connector
3V3  --------> VCC
GND  --------> GND
GPIO21 ------> TX
GPIO22 ------> RX
                CANH ------------> Pin 4
                CANL ------------> Pin 12
                GND  ------------> Pin 1 (pin 9 optional)
```

The Ansix CAN connector also exposes +12V on pin 3 (vehicle-install power feed only, through a buck converter -- see below) and IGN on pin 11 (future enhancement, not wired yet). Full pinout and rationale: [`docs/wiring.md`](docs/wiring.md).

## 8. 3.3V warning

The Adafruit CAN PAL **must** be powered from the same logic voltage as the ESP32 (3.3V here), even though the board itself supports 3-5V.

## 9. Automotive power warning

**Do not power the ESP32 directly from vehicle 12V. Do not power the CAN PAL directly from vehicle 12V.** For a vehicle install, use a proper automotive 12V-to-5V buck converter fed from the Ansix connector's +12V pin (pin 3) into the ESP32's VIN/5V pin; for bench testing, USB power is sufficient (the ESP32's onboard 3.3V regulator powers the CAN PAL). All grounds must be common.

## 10. CAN termination warning

**Set the CAN PAL's termination switch to OFF.** The vehicle CAN network is already terminated at both ends. This ESP32 is a passive monitoring tap, not a new bus endpoint -- adding another termination resistor would drop bus impedance below spec. Confirm the switch position before connecting to the vehicle, and record it in [`docs/wiring.md`](docs/wiring.md).

## 11. Listen-only safety behavior

TWAI is initialized in **listen-only mode** (`TWAI_MODE_LISTEN_ONLY`) and this firmware never transmits a CAN frame, never sends OBD-II or UDS requests, never clears trouble codes, and never attempts ECU configuration. Bus-off recovery (`CanReceiver::handleAlerts()`) reconfigures nothing about the mode -- it only restarts the controller, still listen-only. This is supplemental monitoring equipment; it does not replace factory warnings or driver judgment.

## 12. Arduino CLI installation

```bash
brew install arduino-cli   # or see https://arduino.github.io/arduino-cli/latest/installation/
arduino-cli core install esp32:esp32   # tested against 2.0.17
```

## 13. Board package requirements

FQBN: `esp32:esp32:esp32` (plain ESP32 Dev Module -- the original ESP-WROOM-32, not S3/C3/etc). No third-party libraries are required; only the ESP32 Arduino core's `WiFi`, `esp_now`, and `driver/twai` are used. See [`scripts/common.sh`](scripts/common.sh).

## 14. Build command

```bash
./scripts/build.sh                 # WIRELESS_BENCH_TEST_MODE, per SenderConfig.h (default: on)
./scripts/build.sh --vehicle-can   # force WIRELESS_BENCH_TEST_MODE=0 without editing SenderConfig.h
```

## 15. Upload command

```bash
./scripts/upload.sh /dev/cu.usbserial-XXXX               # find your port: arduino-cli board list
./scripts/upload.sh /dev/cu.usbserial-XXXX --vehicle-can
```

## 16. Monitor command

```bash
./scripts/monitor.sh /dev/cu.usbserial-XXXX   # 115200 baud
```

## 17. How to print the sender MAC

Printed at every boot:

```text
[BOOT] Local MAC: AA:BB:CC:DD:EE:FF
```

Also available anytime over Serial with the `mac` command.

## 18. How to configure the display MAC

Edit [`NetworkConfig.h`](NetworkConfig.h):

```cpp
constexpr uint8_t DISPLAY_MAC[6] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
```

Get the display's MAC from its own boot log (`[BOOT] Local MAC: ...` in the `espNOW-car-gauge` repo) and enter it here. While bringing the display up for the first time, its `NETWORK_DEV_MODE_ACCEPT_ANY_SENDER` can stay `1` so it accepts and logs any sender's MAC -- useful for discovering this sender's real MAC without a chicken-and-egg problem.

## 19. ESP-NOW channel configuration

```cpp
constexpr uint8_t ESPNOW_CHANNEL = 1;  // NetworkConfig.h -- must match the display's NetworkConfig.h
```

## 20. Simulation / wireless bench-test mode

`SenderConfig.h`:

```cpp
#define WIRELESS_BENCH_TEST_MODE 1   // default
```

With this on, TWAI is never initialized (no CAN GPIOs touched, no transceiver assumed present) and `VehicleSimulator` generates a repeating ~50s scenario covering every display state, feeding it through the **exact same** `GaugePacketBuilder` + `EspNowSender` code used in production. See [`docs/validation-plan.md`](docs/validation-plan.md) for the full stage table and the required bench-test procedure/success criteria. Set to `0` (or build with `--vehicle-can`) for real vehicle-CAN operation. A build-time `#warning` fires whenever bench-test mode is enabled, and `SenderConfig.h` has a `static_assert` guard against enabling it in a `PRODUCTION_BUILD`.

## 21. CAN signal table

| Signal | CAN ID | Decode |
| --- | --- | --- |
| Ansix oil pressure | `0x171` | `data[7]`, 1 PSI/count (manufacturer-confirmed) |
| Engine RPM | `0x040` | 14-bit unsigned LE from bit 16 |
| Accelerator position | `0x040` | `data[4] / 2.55` % |
| Engine oil temperature | `0x345` | `data[3] - 40` °C |
| Coolant temperature | `0x345` | `data[4] - 40` °C |
| Steering angle | `0x138` | signed 16-bit LE from `data[2:3]`, `* -0.1` deg |
| Lateral acceleration | `0x13B` | signed 8-bit `data[6]`, `* 0.2` (unit not independently validated) |
| Brake pressure | `0x139` | `data[5] * 128` (unit not independently validated) |

Full detail, confidence levels, and the original RaceChrono equations: [`docs/can/signal-map.md`](docs/can/signal-map.md), [`docs/can/racechrono-profile.md`](docs/can/racechrono-profile.md), [`docs/can/ansix-oil-pressure.md`](docs/can/ansix-oil-pressure.md).

## 22. Signal units and conversions

`VehicleData` stores oil/coolant temperature in Celsius and oil pressure in PSI. The wire protocol wants Fahrenheit for temperature; `GaugePacketBuilder` is the only place that conversion happens (`celsiusToFahrenheit()`). RaceChrono's own `h/0.145` PSI->kPa conversion for oil pressure is deliberately **not** applied -- see [`docs/packet-protocol.md`](docs/packet-protocol.md).

## 23. Validity and timeout behavior

Each signal tracks its own last-updated timestamp and validity flag in `VehicleData`; `VehicleCanDecoder::updateFreshness()` clears a signal's valid flag once it exceeds its configured "lost" timeout (`SignalConfig.h`: 750ms for fast signals/oil pressure, 5000ms for the two temperatures), independent of every other signal -- one stale/invalid signal never invalidates the whole packet. Engine-running state comes from RPM with hysteresis + debounce (400/200 RPM thresholds, 500ms debounce), falling back provisionally to a pressure-based heuristic only when RPM itself is invalid.

## 24. Raw CAN logging

`CanLogger` supports `Disabled` / `AllFrames` / `ChangedFrames` / `SelectedIds` modes, toggled over Serial (`can start`, `can filter changed`, `can filter id 0x171`, ...) or at build time via `CAN_RAW_LOG_ENABLED` in `SenderConfig.h`. Output format:

```text
[CAN] t=124588 id=0x171 std dlc=8 data=00 00 00 00 00 00 00 4A
```

`CanStatistics` (`can summary`) tracks per-ID frame counts, approximate Hz, and last/min/max payload bytes. `FrameHistory` keeps a 256-entry ring buffer of recent frames for the six decoded IDs only (not the whole bus). `scripts/capture_can.sh` tees a monitor session to a timestamped log file.

## 25. Serial commands

```text
help
status
mac
can start | can stop | can summary | can clear
can filter all | can filter changed | can filter id 0xNNN
mark <label>
espnow status | espnow test
test status | test start | test stop | test restart | test next | test previous | test send_once
simulation start | simulation stop | simulation restart   (aliases of test start/stop/restart)
set pressure|rpm|oiltemp|running|pressure_valid|rpm_valid|temp_valid|warning|sensor_error <value>
send once
reboot
```

`set ...` activates a manual fixed-value override (useful for checking exact display thresholds/layouts) that stays active until another `set`, `test restart`, or reboot. `can ...` commands are only available when built with `WIRELESS_BENCH_TEST_MODE=0`; `test`/`simulation ...` only when built with it `=1`.

## 26. Validation procedure

Two phases, in order -- full detail in [`docs/validation-plan.md`](docs/validation-plan.md):

1. **Wireless bench test** (`WIRELESS_BENCH_TEST_MODE=1`, the default): proves ESP-NOW, packet format/CRC, sequence handling, and every display state using simulated data through the production packet path, with no CAN hardware connected at all.
2. **Vehicle CAN validation** (`WIRELESS_BENCH_TEST_MODE=0`): after wiring per `docs/wiring.md`, confirms real frame reception, correct decoding, and plausible values with the engine off/idling/running.

## 27. Current limitations

- Lateral acceleration and brake pressure formulas are implemented exactly as supplied in the RaceChrono profile, but their real-world engineering units and signs have not been independently validated against the vehicle (see `docs/can/signal-map.md`). Fields are named `...Scaled` rather than claiming a unit.
- `PressureWarningLogic`'s RPM/PSI threshold table is a development placeholder, not a validated engine-protection specification, and is independent of the Ansix module's own factory warning behavior.
- Coolant temperature, accelerator position, steering angle, brake pressure, and lateral acceleration are decoded and available internally but not yet transmitted -- the deployed `GaugePacket` (protocol v1) only carries oil pressure, RPM, and oil temperature. See [`docs/packet-protocol.md`](docs/packet-protocol.md) for what a protocol bump would require.
- This firmware has not yet been run against real vehicle CAN traffic (see `docs/can/capture-notes.md` -- no capture sessions logged yet). All signal decoders are covered by host-side unit tests (`tests/`) exercising the formulas against the brief's worked examples, but that validates arithmetic, not real bus behavior.
- No SD card / persistent logging in this version; `FrameHistory` is RAM-only and resets on reboot.

## 28. Warning-threshold disclaimer

`DEVELOPMENT_THRESHOLDS` in `PressureWarningLogic.h` are placeholders "tuned for reasonable on the bench," not a validated safety specification for any particular engine. The Ansix module's own factory oil-pressure warning behavior operates completely independently of this firmware and is the authoritative safety warning. `FLAG_LOW_PRESSURE_WARNING` in the transmitted packet is a supplemental display cue only.

## 29. Future protocol expansion

Any future packet change must: increment `GAUGE_PROTOCOL_VERSION`, keep the packet size explicit (`static_assert(sizeof(GaugePacket) == N, ...)`), update `GaugePacket.h` identically in **both** this repo and `../espNOW-car-gauge`, define what the receiver does on a version mismatch, and recompute the CRC consistently. Candidates for a future version: coolant temperature, accelerator position, steering angle, brake pressure, lateral acceleration (once independently unit-validated).

## 30. Vendor configuration references

- Ansix 2022+ BRZ/GR86 Oil Pressure Sensor CANbus Kit AiM configuration -- mapping confirmed in [`docs/can/ansix-oil-pressure.md`](docs/can/ansix-oil-pressure.md).
- Supplied RaceChrono `vehicleProfile.json` (from a `.rcz` export) -- full contents and per-channel mapping in [`docs/can/racechrono-profile.md`](docs/can/racechrono-profile.md).

Neither vendor file's binary/archive form is redistributed in this repository (not supplied to this repo, and their interpreted mappings are documented instead of parsed at runtime either way).

## Host-side tests

Decoder formulas, CRC/checksum, packet building, and warning-threshold logic are covered by host-side (no Arduino toolchain needed) tests:

```bash
./tests/run_tests.sh
```

## Explicit non-goals

Display rendering, LVGL, touch/rotary input, CAN transmission, OBD-II/UDS requests, ECU programming, DTC clearing, modifying factory warning behavior, a phone app, internet/cloud connectivity, GPS, lap timing, and OTA updates are all out of scope for this repository -- see the project brief for the full rationale.
