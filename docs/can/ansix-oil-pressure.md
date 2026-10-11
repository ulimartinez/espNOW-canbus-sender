# Ansix oil pressure -- manufacturer-confirmed signal

This is the only signal in this project with manufacturer confirmation (Ansix AiM configuration) independently corroborated by the supplied RaceChrono profile -- both agree on the CAN ID and payload byte.

```text
CAN ID:          369 decimal / 0x171 hex
Frame type:      Standard 11-bit
Required DLC:    At least 8
Payload byte:    data[7]
Raw type:        Unsigned 8-bit
Scale:           1 PSI per count
Offset:          0
Output unit:     PSI
```

RaceChrono's own profile applies `h / 0.145` to convert PSI to kPa for its own display -- this firmware does not apply that conversion, since the display protocol's `oilPressurePsi` field wants PSI directly and the raw byte already is PSI.

## Plausibility range

```cpp
constexpr float MIN_VALID_OIL_PRESSURE_PSI = 0.0f;
constexpr float MAX_VALID_OIL_PRESSURE_PSI = 160.0f;
```

Configurable in [`SignalConfig.h`](../../SignalConfig.h). A frame that decodes structurally (right ID, DLC, not extended/remote) but produces a value outside this range is treated as a sensor/wiring fault, not absence of data: `oilPressureValid` is cleared, `sensorError` is set, an invalid-value counter increments, and a throttled warning is logged -- see `VehicleCanDecoder::applyOilPressure()`.

## Supplemental warning thresholds -- explicitly not a validated spec

The Ansix module already runs the vehicle's own factory oil-pressure warning behavior, independently of anything in this firmware. `PressureWarningLogic.h`'s `TRACK_LOG_THRESHOLDS` table is an RPM-interpolated caution/critical curve for a supplemental display cue only -- **not** a substitute for the Ansix module's own warning, and not a manufacturer engine-protection specification.

Unlike the original bench-guess placeholders, this table is derived from two real track logs on this car (before and after an oil pan upgrade) -- see [pressure-threshold-derivation.md](pressure-threshold-derivation.md) for the full methodology, data, and reasoning. Escalating to Caution/Critical is also debounced (`PRESSURE_WARNING_DEBOUNCE_MS` in [`SignalConfig.h`](../../SignalConfig.h)) via `PressureWarningDebouncer`, since the logs show even a healthy car has brief (<0.5s) pressure dips during normal hard cornering -- only a *sustained* drop should surface the warning. Recovery to a less-severe status is never debounced.

Do not raise a critical warning purely from an engine-off zero-pressure reading (`evaluatePressureStatus()` explicitly guards this), and re-derive this table (per the method in pressure-threshold-derivation.md) rather than hand-tuning it if the car, oil pan, or sensor changes.
