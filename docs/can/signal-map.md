# Signal map

| Signal | CAN ID | Decode | Confidence |
| --- | --- | --- | --- |
| Ansix oil pressure | `0x171` | `data[7]`, 1 PSI/count | **Manufacturer-confirmed** (Ansix AiM config + RaceChrono profile agree) |
| Engine RPM | `0x040` | 14-bit unsigned LE starting at bit 16 (`data[2:3] & 0x3FFF`) | Supplied-profile (RaceChrono `bitsToUIntLe(raw, 16, 14)`) |
| Accelerator position | `0x040` | `data[4] / 2.55` % | Supplied-profile (RaceChrono `e/2.55`) |
| Engine oil temperature | `0x345` | `data[3] - 40` °C | Supplied-profile (RaceChrono `D-40`) |
| Coolant temperature | `0x345` | `data[4] - 40` °C | Supplied-profile (RaceChrono `E-40`) |
| Steering angle | `0x138` | signed 16-bit LE from `data[2:3]`, `* -0.1` degrees | Supplied-profile (RaceChrono `bytesToIntLe(raw,2,2) * -0.1`) -- sign convention preserved as-supplied, not independently re-derived |
| Lateral acceleration | `0x13B` | signed 8-bit `data[6]`, `* 0.2` | Supplied-profile formula only -- **final engineering unit (`g`?) not independently validated**. Field is named `lateralAccelerationScaled`, not `...Gs`, until confirmed against a known reference (stationary/straight-line/cornering/phone-accelerometer comparison). |
| Brake pressure | `0x139` | `data[5] * 128` | Supplied-profile formula only -- **final engineering unit not independently validated**. Field is named `brakePressureScaled`, not `...Psi`/`...Kpa`, until confirmed against pedal-pressure behavior. |

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
