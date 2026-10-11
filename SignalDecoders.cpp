#include "SignalDecoders.h"

#include "CanConfig.h"

namespace {
constexpr uint32_t ID_ENGINE = CAN_ID_ENGINE;
constexpr uint32_t ID_STEERING = CAN_ID_STEERING;
constexpr uint32_t ID_BRAKE = CAN_ID_BRAKE;
constexpr uint32_t ID_LATERAL = CAN_ID_LATERAL_ACCEL;
constexpr uint32_t ID_OIL_PRESSURE = CAN_ID_OIL_PRESSURE;
constexpr uint32_t ID_TEMPERATURE = CAN_ID_TEMPERATURE;
}  // namespace

bool decodeOilPressure(const CanFrame &frame, float &pressurePsi) {
    if (frame.extd || frame.rtr || frame.identifier != ID_OIL_PRESSURE || frame.data_length_code < 8) {
        return false;
    }

    pressurePsi = static_cast<float>(frame.data[7]);
    return true;
}

bool decodeEngineRpm(const CanFrame &frame, uint16_t &rpm) {
    if (frame.extd || frame.rtr || frame.identifier != ID_ENGINE || frame.data_length_code < 4) {
        return false;
    }

    const uint16_t raw = static_cast<uint16_t>(frame.data[2]) | (static_cast<uint16_t>(frame.data[3]) << 8);

    rpm = raw & 0x3FFF;
    return true;
}

bool decodeAcceleratorPercent(const CanFrame &frame, float &acceleratorPercent) {
    if (frame.extd || frame.rtr || frame.identifier != ID_ENGINE || frame.data_length_code < 5) {
        return false;
    }

    float value = static_cast<float>(frame.data[4]) / 2.55f;
    if (value > 100.0f) value = 100.0f;  // clamp minor floating-point overrun

    acceleratorPercent = value;
    return true;
}

bool decodeOilTemperatureC(const CanFrame &frame, float &temperatureC) {
    if (frame.extd || frame.rtr || frame.identifier != ID_TEMPERATURE || frame.data_length_code < 5) {
        return false;
    }

    temperatureC = static_cast<float>(frame.data[3]) - 40.0f;
    return true;
}

bool decodeCoolantTemperatureC(const CanFrame &frame, float &temperatureC) {
    if (frame.extd || frame.rtr || frame.identifier != ID_TEMPERATURE || frame.data_length_code < 5) {
        return false;
    }

    temperatureC = static_cast<float>(frame.data[4]) - 40.0f;
    return true;
}

bool decodeSteeringAngleDegrees(const CanFrame &frame, float &steeringDegrees) {
    if (frame.extd || frame.rtr || frame.identifier != ID_STEERING || frame.data_length_code < 4) {
        return false;
    }

    const uint16_t rawUnsigned = static_cast<uint16_t>(frame.data[2]) | (static_cast<uint16_t>(frame.data[3]) << 8);
    const int16_t rawSigned = static_cast<int16_t>(rawUnsigned);

    steeringDegrees = static_cast<float>(rawSigned) * -0.1f;
    return true;
}

bool decodeLateralAcceleration(const CanFrame &frame, float &lateralAccelerationMps2) {
    if (frame.extd || frame.rtr || frame.identifier != ID_LATERAL || frame.data_length_code < 7) {
        return false;
    }

    const int8_t raw = static_cast<int8_t>(frame.data[6]);

    lateralAccelerationMps2 = static_cast<float>(raw) * 0.2f;
    return true;
}

bool decodeBrakePressureScaled(const CanFrame &frame, float &brakePressureScaled) {
    if (frame.extd || frame.rtr || frame.identifier != ID_BRAKE || frame.data_length_code < 6) {
        return false;
    }

    brakePressureScaled = static_cast<float>(frame.data[5]) * 128.0f;
    return true;
}
