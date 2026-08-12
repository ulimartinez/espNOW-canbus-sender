# Wiring

## Hardware

- ESP-WROOM-32 dev board (original ESP32, not ESP8266/S3/C3)
- Adafruit CAN PAL (TJA1051/T3 transceiver), 3-5V capable, switchable termination resistor
- 2022+ Toyota GR86 / Subaru BRZ vehicle CAN bus, tapped at the Ansix CAN connector (see below)

## Vehicle connector -- Ansix CAN connector

The CAN interface connects to the Ansix CAN connector, not a standard OBD-II connector. It originates from the factory Active Sound Control (ASC) connector and reuses an OBD-style shell, but the pinout is Ansix/ASC-specific:

| Pin | Signal | Used |
| --- | --- | --- |
| 1 | Ground | Yes |
| 3 | +12V | Yes (vehicle install power feed only -- see [Power](#power)) |
| 4 | CAN High | Yes |
| 9 | Ground | Optional |
| 11 | IGN | Future enhancement, not wired yet |
| 12 | CAN Low | Yes |
| 15 | Speaker - | No |
| 16 | Speaker + | No |

Only the CAN, power, and ground pins are required for this firmware. Pin 9 (ground) may also be connected if convenient, but pin 1 alone is sufficient.

## ESP32 <-> CAN PAL wiring

```text
ESP32          CAN PAL
3V3  --------> VCC
GND  --------> GND
GPIO21 ------> TX
GPIO22 ------> RX
```

Pin assignments are configurable in [`BoardConfig.h`](../BoardConfig.h):

```cpp
constexpr gpio_num_t CAN_TX_GPIO = GPIO_NUM_21;
constexpr gpio_num_t CAN_RX_GPIO = GPIO_NUM_22;
```

Leave **SLNT disconnected** for the initial implementation. The TJA1051/T3 supports a hardware Silent Mode (receive-only, transmit disabled at the transceiver level) that can be wired to a GPIO as a future enhancement, layered on top of -- not instead of -- the ESP32's own TWAI listen-only mode.

## CAN PAL <-> Ansix wiring

Use the CAN PAL's terminal block:

| CAN PAL | Ansix |
| --- | --- |
| CANH | Pin 4 |
| GND | Pin 1 (pin 9 optional, in addition) |
| CANL | Pin 12 |

The CAN PAL supports 3-5V operation and should be powered from the same logic voltage as the ESP32 (3.3V here) -- **never power it directly from vehicle 12V.**

## CAN termination -- read this before connecting anything

**Set the CAN PAL's termination switch to OFF.** The vehicle CAN network is already terminated at both physical ends by the vehicle's own ECUs/modules. This ESP32 is a monitoring tap, not a new end of the bus -- adding another termination resistor would drop bus impedance below spec.

The firmware assumes termination is disabled; it does not and cannot detect or control this switch.

**Record the physical termination state you found and what you did about it here once bench/vehicle work begins:**

- Board used: _(model/revision, e.g. "Adafruit CAN PAL, TJA1051/T3")_
- Termination switch position confirmed OFF before connecting to the vehicle? _(yes/no)_
- Any notes on bus behavior (fill in during vehicle validation): _(fill in)_

## Power

### Bench testing

Power the ESP32 through USB; its onboard 3.3V regulator powers the CAN PAL:

```text
USB -> ESP32 -> 3V3 -> CAN PAL
```

No vehicle connection is required for bench testing (see [validation-plan.md](validation-plan.md) for the wireless bench test and CAN-PAL-on-the-bench steps, both doable before ever touching the car).

### Vehicle installation

**Do not power the ESP32 directly from vehicle 12V. Do not power the CAN PAL directly from vehicle 12V.** Use a proper automotive 12V-to-5V buck converter, fed from the Ansix connector's +12V pin (pin 3):

```text
Ansix Pin 3 (+12V) -> Automotive Buck Converter (12V->5V) -> ESP32 VIN/5V -> ESP32 3V3 -> CAN PAL
```

All grounds (ESP32, CAN PAL, Ansix connector ground, vehicle chassis ground) must be common.

## Ignition input (future enhancement)

The Ansix connector exposes an IGN signal on pin 11. A future enhancement is to feed IGN through an appropriate voltage divider into an ESP32 GPIO (e.g. GPIO34, using an ADC-capable, input-only-safe pin) to enable ignition detection, wake-from-sleep, a clean engine-off distinction independent of RPM, and lower standby power:

```text
IGN -> Voltage Divider -> GPIO34
```

**Not required for the current implementation** -- RPM already serves as the authoritative engine-running signal (see [`SignalConfig.h`](../SignalConfig.h)). Do not wire IGN until this is implemented in firmware.

## Safety: passive monitoring only

This firmware initializes the ESP32's TWAI controller in **listen-only mode** and never transmits a CAN frame, sends OBD-II/UDS requests, clears trouble codes, or otherwise interacts with vehicle systems. See the main [README](../README.md#safety-and-operating-mode) for the full listen-only safety statement. This is supplemental monitoring equipment and does not replace factory warnings or driver judgment.
