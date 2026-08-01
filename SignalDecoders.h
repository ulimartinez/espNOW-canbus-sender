#pragma once

#include <stdint.h>

// Minimal, host-testable mirror of the fields of twai_message_t that the
// decoders need. Keeping decoders independent of <driver/twai.h> lets them
// be unit tested with a plain g++ host build (see tests/decoder_tests).
// CanReceiver/VehicleCanDecoder convert a real twai_message_t into this
// struct with a trivial field copy.
struct CanFrame {
    uint32_t identifier = 0;
    bool extd = false;
    bool rtr = false;
    uint8_t data_length_code = 0;
    uint8_t data[8] = {0};
};

// Each decoder validates frame ID, RTR/extended-frame rejection, and
// minimum DLC on its own, returning false (leaving the output unmodified)
// for anything that doesn't match. Formulas are taken verbatim from the
// supplied RaceChrono profile / Ansix confirmation -- see
// docs/can/signal-map.md for the confidence level of each.

bool decodeOilPressure(const CanFrame &frame, float &pressurePsi);

bool decodeEngineRpm(const CanFrame &frame, uint16_t &rpm);

bool decodeAcceleratorPercent(const CanFrame &frame, float &acceleratorPercent);

bool decodeOilTemperatureC(const CanFrame &frame, float &temperatureC);

bool decodeCoolantTemperatureC(const CanFrame &frame, float &temperatureC);

bool decodeSteeringAngleDegrees(const CanFrame &frame, float &steeringDegrees);

bool decodeLateralAcceleration(const CanFrame &frame, float &lateralAccelerationScaled);

bool decodeBrakePressureScaled(const CanFrame &frame, float &brakePressureScaled);
