# Capture notes

Log real-vehicle CAN capture sessions here as they happen -- one section per session, so decoder assumptions can be checked against actual bus traffic over time. Use `scripts/capture_can.sh` (or `can start` / `can filter ...` over Serial) to collect raw frames first.

Suggested per-session template:

```markdown
## YYYY-MM-DD -- <car state: idle / driving / etc.>

- Firmware version / commit:
- Duration:
- IDs observed (from `can summary`):
- Anything that didn't match docs/can/signal-map.md:
- Follow-ups:
```

## 2026-08-11 -- ignition on/engine off, then idle (first real-vehicle session)

- Firmware version / commit: `0.1.0`, first commit after the Adafruit CAN PAL hardware swap (see [[hardware-architecture]] project memory)
- Duration: session ran continuously for 550+ seconds across ignition-on/engine-off and idle states
- IDs observed (from `can summary`): **47 unique IDs**, sustained ~750 fps bus load. All six decoded IDs confirmed present as standard frames with the expected DLC=8 and plausible rates:

  | ID | Signal | Observed rate |
  | --- | --- | --- |
  | `0x040` | RPM / accelerator | 100.0 Hz |
  | `0x138` | Steering | 50.0 Hz |
  | `0x139` | Brake | 50.0 Hz |
  | `0x13B` | Lateral accel | 50.0 Hz |
  | `0x171` | Oil pressure | 9.9 Hz |
  | `0x345` | Oil/coolant temp | 10.0 Hz |

- Anything that didn't match `docs/can/signal-map.md`: nothing structural -- every mapped ID/DLC/frame-type assumption held. One minor observation: with the engine off, `oil_valid` flickered on/off every ~1s while values stayed near 0-4 PSI (well inside the 0-160 plausibility range, so not a rejected value -- `sensor_error` stayed 0 throughout). A single cumulative RX-queue-overflow alert (`overflow=1`) was logged once across the whole engine-off period. Once the engine was started and idling (~800-820 RPM), oil pressure read a steady 17-26 PSI with `oil_valid=1` on every sample -- the flicker did not recur at idle. Root cause of the engine-off flicker is not confirmed; leading guesses are either genuine burstiness in the Ansix module's broadcast timing at that vehicle state, or the sender's own `Serial.printf` status line occasionally blocking `loop()` when no host is actively draining the native-USB-CDC TX buffer. Not investigated further since it didn't affect plausibility, self-corrected at idle, and never produced a false pressure reading.
- Follow-ups:
  - Confirm the engine-off flicker really doesn't recur before considering it resolved (was only observed to disappear once, at idle).
  - ESP-NOW: sends failed 100% (`espnow_tx_ok=0`) while the display was off/out of range during the engine-off portion of this session, then succeeded 100% (`espnow_tx_fail` stopped climbing entirely) once the display was powered on -- confirms `EspNowSender`'s recovery/reinit logic works as intended and isn't itself the cause of missed sends.
  - Still to validate per `docs/validation-plan.md` phase 3: accelerator position, steering angle, brake, and lateral acceleration against real pedal/wheel input; oil pressure/RPM behavior during acceleration and shutdown; freshness-timeout behavior on tap disconnect.
