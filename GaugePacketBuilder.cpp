#include "GaugePacketBuilder.h"

uint16_t GaugePacketBuilder::buildFlags(const VehicleData &data, PressureStatus pressureStatus) {
    uint16_t flags = 0;

    if (data.oilPressureValid) flags |= FLAG_PRESSURE_VALID;
    if (data.rpmValid) flags |= FLAG_RPM_VALID;
    if (data.oilTemperatureValid) flags |= FLAG_OIL_TEMP_VALID;
    if (data.engineRunning) flags |= FLAG_ENGINE_RUNNING;
    if (data.sensorError) flags |= FLAG_SENSOR_ERROR;

    // Only assert the low-pressure warning when the supplemental warning
    // logic actually ran on valid inputs -- never on Unknown/SensorError.
    if (pressureStatus == PressureStatus::Critical) flags |= FLAG_LOW_PRESSURE_WARNING;

    return flags;
}

GaugePacket GaugePacketBuilder::build(const VehicleData &data, uint32_t sequence, uint32_t senderUptimeMs) {
    PressureStatus pressureStatus = pressureWarningDebouncer_.evaluate(data.sensorError, data.oilPressureValid,
                                                                        data.oilPressurePsi, data.rpmValid, data.rpm,
                                                                        data.engineRunning, senderUptimeMs);

    GaugePacket packet{};
    packet.sequence = sequence;
    packet.senderUptimeMs = senderUptimeMs;

    packet.oilPressurePsi = data.oilPressureValid ? data.oilPressurePsi : 0.0f;
    packet.oilTemperatureF = data.oilTemperatureValid ? celsiusToFahrenheit(data.oilTemperatureC) : 0.0f;
    packet.rpm = data.rpmValid ? data.rpm : 0;
    packet.flags = buildFlags(data, pressureStatus);

    gaugePacketSign(packet);  // fills magic/protocolVersion/packetSize/checksum
    return packet;
}
