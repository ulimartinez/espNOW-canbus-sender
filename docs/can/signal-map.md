# Signal map

| Signal | CAN ID | Decode | Confidence |
| --- | --- | --- | --- |
| Ansix oil pressure | `0x171` | `data[7]`, 1 PSI/count | **Manufacturer-confirmed** (Ansix AiM config + RaceChrono profile agree) |
| Engine RPM | `0x040` | 14-bit unsigned LE starting at bit 16 (`data[2:3] & 0x3FFF`) | Supplied-profile (RaceChrono `bitsToUIntLe(raw, 16, 14)`) |
| Accelerator position | `0x040` | `data[4] / 2.55` % | Supplied-profile (RaceChrono `e/2.55`) |
| Engine oil temperature | `0x345` | `data[3] - 40` °C | Supplied-profile (RaceChrono `D-40`) |
| Coolant temperature | `0x345` | `data[4] - 40` °C | Supplied-profile (RaceChrono `E-40`) |
| Steering angle | `0x138` | signed 16-bit LE from `data[2:3]`, `* -0.1` degrees | Supplied-profile (RaceChrono `bytesToIntLe(raw,2,2) * -0.1`) -- sign convention preserved as-supplied, not independently re-derived |
| Lateral acceleration | `0x13B` | signed 8-bit `data[6]`, `* 0.2` **m/s²** (÷ 9.80665 for g) | **Validated against real track logs** -- see "Lateral acceleration validation" below. Sign convention (which direction is positive) is *not* verified. |
| Brake pressure | `0x139` | `data[5] * 128` | Supplied-profile formula only -- **final engineering unit not independently validated**. Field is named `brakePressureScaled`, not `...Psi`/`...Kpa`, until confirmed against pedal-pressure behavior. |

| Vehicle speed | -- | **not decoded** | No validated source. See "Vehicle speed" below. Packet v2 has a field and `FLAG_SPEED_VALID` for it, but the flag is never set. |

"Supplied-profile" means: the formula is exactly what the supplied RaceChrono `vehicleProfile.json` specifies for this CAN ID/byte range, and is implemented verbatim. It does **not** mean the resulting engineering unit or sign convention has been independently verified against the real vehicle -- only oil pressure has manufacturer confirmation for both the payload location and the unit.

## CAN ID decimal/hex reference

RaceChrono's `vehicleProfile.json` uses decimal CAN identifiers; this firmware (and the CAN bus itself) uses hex.

| Decimal | Hex |
| --- | --- |
| 64 | `0x040` |
| 312 | `0x138` |
| 313 | `0x139` |
| 315 | `0x13B` |
| 369 | `0x171` |
| 837 | `0x345` |

## Byte-labeling convention

RaceChrono equations label bytes alphabetically (`A`/`a` = `data[0]` ... `H`/`h` = `data[7]`); this firmware always writes decoders in terms of `data[N]` directly. See [racechrono-profile.md](racechrono-profile.md) for the original equations as supplied.

## Frame validation applied by every decoder

All eight decoders reject: extended (29-bit) frames, remote frames, and frames with insufficient DLC for the fields they read. See [`SignalDecoders.cpp`](../../SignalDecoders.cpp).

## Lateral acceleration validation

RaceChrono applies the profile's `bytesToIntLe(raw, 6, 1) * 0.2` and then labels the channel in **G** -- so its exported CAN lateral-acceleration values are all exact multiples of `0.2 / 9.80665` (e.g. `-0.06118` G = `-0.6` m/s²). In other words, the raw `* 0.2` result is **m/s²**, and RaceChrono converts it to g itself.

Cross-checked against four RaceChrono track logs on this car (2025-09-20, 2026-05-09, 2026-07-12 ×2; ~83,000 samples total), each of which also records the phone's own accelerometer (`y_acc`, G) and GPS-derived lateral acceleration (`calc`, G):

| Comparison | Result |
| --- | --- |
| Correlation, CAN vs phone accelerometer | 0.998 in every session |
| Correlation, CAN vs GPS-derived | 0.995-0.996 |
| Least-squares slope, CAN (as g) vs phone | 1.04 |
| 99th-percentile \|lat\| (one session): CAN / phone / GPS | 1.44 / 1.35 / 1.29 g |
| Single highest sample, CAN | 1.77 g -- an isolated transient (curb strike); a 250 ms rolling median of the same data peaks at 1.45 g |

Conclusion: the decode is correct and the unit is m/s². `VehicleData::lateralAccelerationMps2` holds it; `GaugePacketBuilder` converts to g for the wire. Because single-sample transients reach far above sustained cornering load, anything tracking peaks (the display's drive statistics) must reject isolated spikes.

## Vehicle speed

None of the supplied RaceChrono profile's channels is vehicle speed, and the track logs' speed columns are GPS-derived, so there is no data on this car to validate a CAN speed decoder against. Community GR86/BRZ references point at `0x139`, but the mapping recalled from them (13 bits from bit 16, `* 0.015625` km/h) **cannot be correct** -- it saturates at 128 km/h, while the logs show 125+ mph. Rather than ship a guessed decoder, speed is left undecoded and the display shows `N/A`.

To validate one:

1. Run RaceChrono (GPS speed) and this sender's raw CAN capture (`scripts/capture_can.sh`, or `can filter 139` + `can start` over Serial) at the same time on a drive that covers 0 to highway speed.
2. For `0x139` (and any other candidate ID), correlate each 16-bit little-endian byte pair -- and its low 12/13/14/15 bits -- against GPS speed. The right field will correlate > 0.99 and have a constant ratio; that ratio is the scale.
3. Add a `decodeVehicleSpeedKph()` to `SignalDecoders` with a decoder test, a plausibility limit in `SignalConfig.h`, populate `VehicleData`, and set `FLAG_SPEED_VALID` in `GaugePacketBuilder`. No display change is needed.
