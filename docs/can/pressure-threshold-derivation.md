# Pressure warning threshold derivation

`TRACK_LOG_THRESHOLDS` in [`PressureWarningLogic.h`](../../PressureWarningLogic.h) and the debounce duration in [`SignalConfig.h`](../../SignalConfig.h) were derived from two real RaceChrono/OBDLink MX+ track logs on this specific car, using the same Ansix oil-pressure sensor this firmware reads. This is still **not a validated engine-protection specification** -- see the disclaimer in `PressureWarningLogic.h` and `docs/can/ansix-oil-pressure.md` -- but it's a meaningful upgrade over an arbitrary bench guess, and the methodology here is documented so it can be redone if the car, sensor, or oil system changes again.

## Source data

- **2025 session** (before an oil pan upgrade): `session_20250920_143337_ridge_lap7_1_v3.csv`, ~17,400 data rows.
- **2026 session** (after the upgrade): `session_20260712_095446_ridge_lap6_v3.csv`, ~22,900 data rows.

Both are RaceChrono Pro CSV exports. The `engine_oil_pressure` column is in **kPa** (RaceChrono's own profile applies a `h/0.145` PSI->kPa conversion for its display -- see `docs/can/racechrono-profile.md`), converted back to PSI (`* 0.145037738`) to match this firmware's units. Neither log contains any idle/low-RPM data (both are hot-lap sessions), so the 800/2000 RPM table points are unchanged bench placeholders -- there's nothing to derive them from yet.

## Method

1. Bucket every sample by RPM and compute percentiles of oil pressure per bucket, per session -- see "RPM-bucketed percentiles" below.
2. Separately, find every contiguous run where pressure drops below a threshold at RPM >= 4500, and measure each run's duration and minimum -- see "Dip-event analysis" below. This is what actually drove the debounce decision: percentiles alone don't tell you whether a low reading was a 50ms blip or a 2-second sustained drop.
3. Set `criticalPsi` per RPM point low enough to sit below the worst *sustained* dip observed, but not so low it stops meaning anything; set `cautionPsi` as an earlier warning zone above that.

## RPM-bucketed percentiles (PSI)

| RPM bucket | 2025 min / p1 | 2026 min / p1 |
| --- | --- | --- |
| 3000-4000 | 28.0 / 28.3 | 22.0 / 22.0 |
| 4000-5000 | 29.0 / 31.0 | 14.0 / 17.3 |
| 5000-6000 | 7.0 / 19.6 | 27.5 / 32.7 |
| 6000-6500 | 36.0 / 37.3 | 25.0 / 33.6 |
| 6500-7000 | 20.0 / 23.4 | 32.0 / 34.2 |
| 7000-7500 | 33.0 / 34.0 | 32.0 / 35.2 |
| 7500-8000 | 44.6 / 44.7 (n=40) | 39.4 / 39.5 (n=49) |

The 5000-6500 RPM band is the clear pre-upgrade trouble spot: the 2025 floor drops as low as 7 PSI, while 2026 stays a comfortable 25+ PSI in the same band. Everywhere else, the two sessions look similar.

## Dip-event analysis

Contiguous runs below 25 PSI at RPM >= 4500 (every event, not just percentiles):

**2025 (no upgraded pan): 8 events**

| Duration | Min PSI | RPM range |
| --- | --- | --- |
| 0.31s | 7.0 | 5377-5535 |
| 0.46s | 13.0 | 5438-5522 |
| 0.37s | 19.0 | 5602-5765 |
| 0.32s | 20.0 | 6939-6972 |
| 0.16s | 22.0 | 5465-5564 |
| 0.15s | 22.0 | 5468-5574 |
| 0.10s | 23.0 | 5573-5584 |
| 0.06s | 23.0 | 5702-5716 |

**2026 (upgraded pan): 3 events**

| Duration | Min PSI | RPM range |
| --- | --- | --- |
| 0.26s | 17.0 | 4501-4584 |
| 0.09s | 21.0 | 4813-4943 |
| 0.00s | 22.9 | 4504-4505 |

Two things drove the debounce decision:

- **Every single event in both sessions lasted under half a second** -- including the worst 7 PSI dip. There is no sustained (multi-second) low-pressure event in either log.
- The pan upgrade didn't just raise the floor, it **eliminated most of the events** (8 -> 3) and **shifted them to a lower RPM range** (5377-6972 -> 4501-4943) -- consistent with the reported symptom being specifically an RPM/cornering-load-dependent surge issue that the pan fixes.

Given brief sub-second dips happen even on the improved car during normal hard cornering, and the user's own stated intent is to react (lift off-throttle) only to a *sustained* drop while staying on-throttle through brief flicker, `PRESSURE_WARNING_DEBOUNCE_MS = 200` was chosen to sit below the shortest "real" 2025 event duration (0.31s) while filtering out sub-200ms noise/normal transients.

## Resulting table

| RPM | Caution PSI | Critical PSI | Basis |
| --- | --- | --- | --- |
| 800 | 10 | 6 | unchanged bench placeholder -- no logged data |
| 2000 | 25 | 18 | unchanged bench placeholder -- no logged data |
| 4000 | 28 | 15 | 2026 floor ~14-22; 2025 floor ~28-29 |
| 5000 | 25 | 15 | catches 2025's 7/13 PSI events as Critical; 19-23 PSI events land in Caution; 2026 stays healthy (min 27.5) |
| 6000 | 32 | 20 | 2026's worst single point (25) lands in Caution, not Critical; 2025 healthy (36+) here |
| 6500 | 28 | 18 | catches 2025's 20 PSI dip as Caution; 2026 healthy (32+) |
| 7000 | 32 | 22 | both sessions healthy (~32-35 floor) |
| 7500 | 42 | 32 | both sessions healthy (~39-45 floor), small sample (n=40-49) |

The curve is intentionally non-monotonic (it dips in the 5000-6500 band) -- `interpolatePressureThresholds()` supports this fine, and it reflects the real, RPM-localized surge tendency rather than a smoothed-over approximation of it.

## Re-deriving this

If the car, engine, oil pan, or sensor changes, or more track logs become available (more laps, other tracks, hotter oil temps, etc.), redo this analysis rather than hand-tuning the existing table -- the scratch scripts used aren't checked into this repo (they were one-off analysis, not firmware), but the method above is enough to reproduce them: bucket by RPM, compute low percentiles, and separately measure the duration of any low-pressure excursions rather than trusting a bare minimum/percentile alone.
