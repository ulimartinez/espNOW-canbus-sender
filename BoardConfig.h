#pragma once

#include <stdint.h>

#include <driver/gpio.h>

// ESP-WROOM-32 dev board wiring to the Adafruit CAN PAL (TJA1051/T3) CAN
// transceiver. 3.3V logic throughout. Do NOT power the transceiver from 5V.
// See docs/wiring.md for the full harness and termination notes.
//
// The CAN PAL's SLNT pin (hardware Silent Mode) is left disconnected for
// now -- ESP32-side TWAI listen-only mode is the enforced safety boundary.
// Wiring SLNT to a GPIO to additionally silence the transceiver itself is a
// documented future enhancement, not required for the current firmware.

constexpr gpio_num_t CAN_TX_GPIO = GPIO_NUM_21;  // -> CAN PAL TX
constexpr gpio_num_t CAN_RX_GPIO = GPIO_NUM_22;  // <- CAN PAL RX

constexpr uint32_t SERIAL_BAUD_RATE = 115200;
