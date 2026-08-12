# Validation plan

Three phases, in order. **Do not skip straight to the vehicle** -- phases 1 and 2 are both doable on the bench, with no CAN hardware (phase 1) or no vehicle (phase 2) involved, and catch most problems far more cheaply than debugging in the car.

## Phase 1: wireless bench test (`WIRELESS_BENCH_TEST_MODE=1`, the default)

Validates ESP-NOW init, MAC/channel configuration, packet format/CRC compatibility, sequence handling, and every display state -- using the exact production packet-building and ESP-NOW code, with `VehicleSimulator` standing in for CAN-decoded data. No CAN transceiver or vehicle connection required at all.

### Procedure

1. Flash the display firmware (`../espNOW-car-gauge`), power it, and read its Wi-Fi MAC from its boot log (`[BOOT] Local MAC: ...`).
2. Set that MAC as `DISPLAY_MAC` in [`NetworkConfig.h`](../NetworkConfig.h); confirm `ESPNOW_CHANNEL` matches the display's `NetworkConfig.h`.
3. Build and flash this repo with `WIRELESS_BENCH_TEST_MODE=1` (default): `./scripts/upload.sh <port>`.
4. Power both devices with no CAN hardware connected.
5. Open the sender's Serial monitor (`./scripts/monitor.sh <port>`).
6. Confirm `[ESPNOW] Peer registered` and the sender/receiver MAC/channel lines print correctly at boot.
7. Confirm `espnow status` shows increasing `ok` counts and the display shows live data.
8. Step through (or let it free-run) the stage table below and confirm the corresponding display behavior for each stage.
9. Confirm `test restart` / `test next` / `test previous` work and sequence numbers keep climbing (they do not reset per stage or per scenario loop).
10. Confirm neither ESP32 needs a reboot to recover after the simulated wireless-loss stage.
11. Only after all of this passes, proceed to phase 2.

### Bench-test scenario stages

One full cycle is ~50 seconds; it repeats indefinitely. See [`VehicleSimulator.cpp`](../VehicleSimulator.cpp).

| # | Stage | ~Duration | Oil pressure | RPM | Engine running | Notable flags | Expected display behavior |
| - | --- | --- | --- | --- | --- | --- | --- |
| 1 | `NO_VEHICLE_DATA` | 3s | invalid | invalid | false | none valid | Sender-online-but-no-data (distinct from wireless loss) |
| 2 | `IGNITION_ON_ENGINE_OFF` | 3s | 0 PSI (valid) | 0 (valid) | false | pressure/rpm/temp valid | No false low-pressure warning at rest |
| 3 | `ENGINE_START` | 2s | 0->75 PSI ramp | 0->1400 ramp | true | -- | Smooth ramp, no discontinuity |
| 4 | `COLD_IDLE` | 5s | ~70 PSI wobble | ~1250 wobble | true | Normal | Live wobble visible |
| 5 | `WARM_UP` | 8s | 40->25 PSI | ~900 | true | Normal | Oil temp climbs 100->190°F |
| 6 | `ACCELERATION` | 6s | 30->85 PSI | 1000->7200 | true | Normal | Smooth ramp |
| 7 | `HIGH_RPM_NORMAL` | 4s | ~75 PSI wobble | ~6950 wobble | true | Normal | -- |
| 8 | `CAUTION_PRESSURE` | 3s | 45 PSI | 6500 | true | supplemental Caution (no wire flag) | -- |
| 9 | `CRITICAL_LOW_PRESSURE` | 3s | ramps to ~22 PSI | 6500 | true | `FLAG_LOW_PRESSURE_WARNING` | Critical warning screen, pressure still visible |
| 10 | `RECOVERY` | 4s | ramps to 65 PSI | ramps to 5000 | true | warning clears | Display exits warning state |
| 11 | `SENSOR_ERROR` | 3s | **invalid**, not fabricated | valid | true | `FLAG_SENSOR_ERROR` | Distinct error state, not confused with low-pressure/engine-off |
| 12 | `CAN_DATA_LOSS` | 3s | invalid | invalid | false | none valid, still transmitting | Sender-online-no-data again (packets keep arriving) |
| 13 | `WIRELESS_LOSS` | 3s | n/a -- **no packets sent at all** | -- | -- | -- | Display's own stale-packet timeout (`DATA_LOST_TIMEOUT_MS`) fires |
| 14 | `SENDER_RECOVERY` | 3s | 30 PSI | 900 | true | all valid | Display recovers automatically, no reboot |
| 15 | `ENGINE_SHUTDOWN` | 4s | ramps to 0 | ramps to 0 | false near end | warning stays clear | No false critical warning as pressure falls with the engine off |

### Required success criteria (from the project brief)

- Peer registers; display receives packets with no Wi-Fi router present.
- Magic/version/CRC accepted.
- Pressure/RPM/temperature update smoothly; validity flags affect the UI correctly.
- Engine-off zero pressure never triggers a false warning.
- Critical warning triggers and clears correctly.
- Sensor-error state is visually distinct from low-pressure and from no-data.
- Stopping transmission produces a communication-loss state; resuming restores it automatically.
- Sequence counters climb monotonically (modulo natural rollover); send failures are visible in `espnow status`.

## Phase 2: CAN PAL bench validation (`WIRELESS_BENCH_TEST_MODE=0`, no vehicle)

Proves the real CAN hardware path -- TWAI driver init, the Adafruit CAN PAL wiring, listen-only mode -- works before it's ever near the car. The ESP32 + CAN PAL are powered over USB only; nothing is connected to the Ansix connector yet.

1. Complete the ESP32 <-> CAN PAL wiring and termination-switch check in [wiring.md](wiring.md) (termination switch OFF).
2. Build with `./scripts/build.sh --vehicle-can` (or set `WIRELESS_BENCH_TEST_MODE 0` in `SenderConfig.h`) and upload.
3. Power the ESP32 over USB only -- do not connect CANH/CANL to anything yet.
4. Confirm TWAI initializes at 500 kbit/s in listen-only mode (`[CAN] TWAI initialized: ... LISTEN-ONLY`) with no driver install/start errors.
5. Confirm `status` / `can summary` show zero frames received (expected, with no bus connected) and no TWAI alert spam (`[CAN][ALERT] ...`) -- an idle, unterminated, disconnected bus can otherwise generate bus-error noise; if it does, it should still recover cleanly via `CanReceiver::handleAlerts()` without needing a reboot.
6. Confirm ESP-NOW still transmits status packets on schedule throughout (CAN being silent must never block or slow the ESP-NOW send loop).

Only proceed to phase 3 once this is clean -- it isolates "is the CAN hardware/firmware path healthy" from "is the vehicle signal what we expect," which are much easier to debug separately.

## Phase 3: vehicle CAN validation

1. Complete the wiring in [wiring.md](wiring.md): Ansix connector pinout, CAN PAL terminal block, termination switch OFF, and (for an installed, non-bench power setup) the 12V-to-5V buck converter path -- **before** connecting to the vehicle.
2. Power the sender safely; do not connect the ESP32 or CAN PAL directly to vehicle 12V.
3. With the ignition on, use `can start` / `can summary` to confirm frames on `0x040`, `0x138`, `0x139`, `0x13B`, `0x171`, `0x345` appear, as standard frames, with the expected DLCs.
4. Confirm oil pressure reads near zero with the engine off.
5. Start the engine; confirm oil pressure rises and RPM tracks actual engine speed.
6. Confirm accelerator position changes with pedal input.
7. Confirm oil/coolant temperature rise plausibly during warm-up.
8. Confirm steering angle changes with wheel movement (sign convention as currently implemented -- see [signal-map.md](can/signal-map.md); document if inverted from expectation, do not silently "fix" it without noting the change).
9. Confirm the brake channel changes with pedal pressure and returns to baseline.
10. Confirm lateral acceleration stays near baseline while stationary.
11. Confirm the display receives real values (switch to phase-1-style checks against the display).
12. Shut the engine off; confirm RPM and pressure fall toward zero and the engine-running flag clears (with debounce).
13. Confirm signal timeout behavior (freshness lost timeouts in [`SignalConfig.h`](../SignalConfig.h)) by disconnecting the CAN tap and observing validity flags clear.

**Do not conduct extended stationary high-RPM testing** -- validate steering/lateral-acceleration/brake signals during normal, brief, safe driving only.

Log what you find in [`docs/can/capture-notes.md`](can/capture-notes.md).
