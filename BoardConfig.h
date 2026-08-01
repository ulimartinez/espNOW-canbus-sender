#pragma once

#include <stdint.h>

#include <driver/gpio.h>

// ESP-WROOM-32 dev board wiring to the Waveshare SN65HVD230 CAN transceiver.
// 3.3V logic throughout. Do NOT power the transceiver from 5V. See
// docs/wiring.md for the full harness and termination notes.

constexpr gpio_num_t CAN_TX_GPIO = GPIO_NUM_21;  // -> SN65HVD230 TXD
constexpr gpio_num_t CAN_RX_GPIO = GPIO_NUM_22;  // <- SN65HVD230 RXD

constexpr uint32_t SERIAL_BAUD_RATE = 115200;
