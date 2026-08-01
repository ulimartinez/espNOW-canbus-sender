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

The Ansix module already runs the vehicle's own factory oil-pressure warning behavior, independently of anything in this firmware. `PressureWarningLogic.h`'s `DEVELOPMENT_THRESHOLDS` table is a **placeholder** RPM-interpolated caution/critical curve for a supplemental display cue only:

```cpp
constexpr PressureThresholdPoint DEVELOPMENT_THRESHOLDS[] = {
    {  800, 10.0f,  6.0f },
    { 2000, 25.0f, 18.0f },
    { 4000, 40.0f, 30.0f },
    { 6000, 50.0f, 38.0f },
    { 7500, 55.0f, 42.0f },
};
```

These numbers are tuned for "reasonable on the bench," not derived from or verified against any manufacturer engine-protection specification. Do not treat `FLAG_LOW_PRESSURE_WARNING` as a substitute for the factory warning light, and do not raise a critical warning purely from an engine-off zero-pressure reading (`evaluatePressureStatus()` explicitly guards this).
