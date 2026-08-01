#pragma once

#include <stdint.h>

#include "GaugePacket.h"
#include "PressureWarningLogic.h"
#include "VehicleData.h"

// Builds the exact display-protocol GaugePacket (see GaugePacket.h, copied
// verbatim from the display repo) from VehicleData. This is the ONLY place
// unit conversion for the wire format happens (PSI is already the wire
// unit for pressure; oil temperature is Celsius internally but the packet
// wants Fahrenheit). Used identically by both the real CAN pipeline and
// the wireless bench-test simulator, so both paths are proven against the
// same code that ships to the vehicle.
class GaugePacketBuilder {
public:
    // sequence is incremented by the caller (EspNowSender) for every
    // attempted transmission, not owned here.
    GaugePacket build(const VehicleData &data, uint32_t sequence, uint32_t senderUptimeMs) const;

private:
    static uint16_t buildFlags(const VehicleData &data, PressureStatus pressureStatus);
};
