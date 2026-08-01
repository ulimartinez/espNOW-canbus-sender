# Wiring

## Hardware

- ESP-WROOM-32 dev board (original ESP32, not ESP8266/S3/C3)
- Waveshare SN65HVD230 CAN transceiver board
- 2022+ Toyota GR86 / Subaru BRZ vehicle CAN bus (via the Ansix oil-pressure CAN module's tap point, or any accessible CAN-H/CAN-L pair on the same network)

## Connections

```text
ESP-WROOM-32             Waveshare SN65HVD230

3V3  ------------------> VCC / 3.3V
GND  ------------------> GND
GPIO21 ----------------> TXD
GPIO22 <---------------- RXD

                          CANH ---> Vehicle CAN High
                          CANL ---> Vehicle CAN Low
```

Pin assignments are configurable in [`BoardConfig.h`](../BoardConfig.h):

```cpp
constexpr gpio_num_t CAN_TX_GPIO = GPIO_NUM_21;
constexpr gpio_num_t CAN_RX_GPIO = GPIO_NUM_22;
```

**The Waveshare board must be powered from 3.3V, never 5V.** All grounds (ESP32, transceiver, vehicle chassis ground used for the CAN tap) must be common.

**Do not power the ESP32 directly from vehicle 12V.** Use a proper automotive 12V-to-5V buck converter (feeding the ESP32's 5V/VIN pin) or a USB power source. Automotive electrical systems have load-dump transients and reverse-polarity risk that a bare linear regulator will not survive.

## CAN termination -- read this before connecting anything

**Do not add a 120-ohm termination resistor.** The vehicle CAN network is already terminated at both physical ends by the vehicle's own ECUs/modules. This ESP32 is a monitoring tap, not a new end of the bus.

Before connecting the Waveshare board to the vehicle:

1. Disconnect and unpower the Waveshare board.
2. Measure resistance between CANH and CANL on the board's terminals.
3. Determine whether the board has an onboard 120-ohm termination resistor (some Waveshare SN65HVD230 boards ship with a solder-jumper-enabled termination resistor).
4. If a termination resistor is present and enabled, disable/remove it unless you have independently confirmed the vehicle harness at your tap point specifically requires it (it should not, for a passive tap on an already-terminated bus).

**Record the physical termination state you found and what you did about it here once bench/vehicle work begins:**

- Board used: _(model/revision, e.g. "Waveshare SN65HVD230 CAN Board")_
- Measured CANH-CANL resistance before any changes: _(fill in)_
- Onboard termination resistor present? _(yes/no)_
- Action taken: _(left disabled / removed / left as shipped, and why)_
- Measured CANH-CANL resistance after changes, board connected to vehicle: _(fill in -- should read close to 60 ohms, i.e. two ~120-ohm terminations in parallel, if the vehicle's own termination is intact and this tap adds none)_

## Safety: passive monitoring only

This firmware initializes the ESP32's TWAI controller in **listen-only mode** and never transmits a CAN frame, sends OBD-II/UDS requests, clears trouble codes, or otherwise interacts with vehicle systems. See the main [README](../README.md#safety-and-operating-mode) for the full listen-only safety statement. This is supplemental monitoring equipment and does not replace factory warnings or driver judgment.
