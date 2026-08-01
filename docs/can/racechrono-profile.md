# Supplied RaceChrono profile

The vehicle-side profile supplied for this project (`vehicleProfile.json`, from a RaceChrono `.rcz` export for the 2022+ Subaru BRZ / Toyota GR86) defines these custom channels:

```json
[
  { "pid": 837, "equation": "D-40", "channelId": 10066, "deviceType": 12 },
  { "pid": 315, "equation": "bytesToIntLe(raw, 6, 1) * 0.2", "channelId": 7, "deviceType": 12 },
  { "pid": 64, "equation": "bitsToUIntLe(raw, 16, 14)", "channelId": 10024, "deviceType": 12 },
  { "pid": 313, "equation": "F * 128", "channelId": 1033, "deviceType": 12 },
  { "pid": 312, "equation": "bytesToIntLe(raw, 2, 2) * -0.1", "channelId": 1001, "deviceType": 12 },
  { "pid": 64, "equation": "e/2.55", "channelId": 10025, "deviceType": 12 },
  { "pid": 837, "equation": "E-40", "channelId": 10026, "deviceType": 12 },
  { "pid": 369, "equation": "h/0.145", "channelId": 1007, "deviceType": 12 }
]
```

`pid` is decimal CAN ID; `equation` is RaceChrono's own expression syntax operating on `raw` (the full data payload) or on alphabetically-labeled single bytes (`A`/`a` = `data[0]` ... `H`/`h` = `data[7]`).

## How each equation maps to this firmware

| pid (hex) | Equation | Meaning | This firmware |
| --- | --- | --- | --- |
| 837 (`0x345`) | `D-40` | `data[3] - 40` = oil temperature, °C | `decodeOilTemperatureC()` |
| 837 (`0x345`) | `E-40` | `data[4] - 40` = coolant temperature, °C | `decodeCoolantTemperatureC()` |
| 315 (`0x13B`) | `bytesToIntLe(raw, 6, 1) * 0.2` | signed byte at offset 6, `* 0.2` | `decodeLateralAcceleration()` |
| 64 (`0x040`) | `bitsToUIntLe(raw, 16, 14)` | 14-bit unsigned LE starting at bit 16 (= `data[2:3] & 0x3FFF`) | `decodeEngineRpm()` |
| 64 (`0x040`) | `e/2.55` | `data[4] / 2.55` = accelerator, % | `decodeAcceleratorPercent()` |
| 313 (`0x139`) | `F * 128` | `data[5] * 128` | `decodeBrakePressureScaled()` |
| 312 (`0x138`) | `bytesToIntLe(raw, 2, 2) * -0.1` | signed 16-bit LE at offset 2, `* -0.1` = steering, degrees | `decodeSteeringAngleDegrees()` |
| 369 (`0x171`) | `h/0.145` | `data[7] / 0.145` -- RaceChrono's own PSI->kPa conversion for display in its own UI | **Not applied.** This firmware transmits `data[7]` directly as PSI (see [ansix-oil-pressure.md](ansix-oil-pressure.md)); it is manufacturer-confirmed that the raw byte is already 1 PSI/count. |

All formulas above are implemented **verbatim** as supplied. That is a statement about formula fidelity, not about independent validation of the resulting real-world unit or sign -- see the confidence column in [signal-map.md](signal-map.md).
