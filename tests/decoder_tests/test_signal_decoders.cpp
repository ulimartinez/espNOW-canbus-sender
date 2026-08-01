#include <initializer_list>

#include "../../SignalDecoders.h"
#include "../test_framework.h"

namespace {

CanFrame makeFrame(uint32_t id, uint8_t dlc, std::initializer_list<uint8_t> bytes, bool extd = false,
                    bool rtr = false) {
    CanFrame f;
    f.identifier = id;
    f.extd = extd;
    f.rtr = rtr;
    f.data_length_code = dlc;
    uint8_t i = 0;
    for (uint8_t b : bytes) f.data[i++] = b;
    return f;
}

void testOilPressure() {
    // ID 0x171, data[7] = 74 -> 74 PSI (brief's confirmed example).
    CanFrame f = makeFrame(0x171, 8, {0, 0, 0, 0, 0, 0, 0, 74});
    float psi = -1.0f;
    CHECK(decodeOilPressure(f, psi));
    CHECK_NEAR(psi, 74.0f, 0.001);

    // Wrong ID must be rejected.
    CanFrame wrongId = makeFrame(0x172, 8, {0, 0, 0, 0, 0, 0, 0, 74});
    CHECK(!decodeOilPressure(wrongId, psi));

    // Extended frame with the same numeric ID must be rejected.
    CanFrame ext = makeFrame(0x171, 8, {0, 0, 0, 0, 0, 0, 0, 74}, /*extd=*/true);
    CHECK(!decodeOilPressure(ext, psi));

    // Remote frame must be rejected.
    CanFrame rtr = makeFrame(0x171, 8, {0, 0, 0, 0, 0, 0, 0, 74}, false, /*rtr=*/true);
    CHECK(!decodeOilPressure(rtr, psi));

    // Insufficient DLC (needs >= 8) must be rejected.
    CanFrame shortDlc = makeFrame(0x171, 7, {0, 0, 0, 0, 0, 0, 74});
    CHECK(!decodeOilPressure(shortDlc, psi));
}

void testEngineRpm() {
    // 14-bit LE value starting at data[2]. Use a known value: 0x1234 & 0x3FFF = 0x1234 (already < 0x4000).
    uint16_t raw = 0x1234;
    CanFrame f = makeFrame(0x040, 4, {0, 0, static_cast<uint8_t>(raw & 0xFF), static_cast<uint8_t>(raw >> 8)});
    uint16_t rpm = 0;
    CHECK(decodeEngineRpm(f, rpm));
    CHECK_EQ(rpm, raw & 0x3FFF);

    // Top two bits must be masked off.
    uint16_t rawWithGarbageBits = 0xFFFF;
    CanFrame f2 = makeFrame(0x040, 4,
                            {0, 0, static_cast<uint8_t>(rawWithGarbageBits & 0xFF),
                             static_cast<uint8_t>(rawWithGarbageBits >> 8)});
    CHECK(decodeEngineRpm(f2, rpm));
    CHECK_EQ(rpm, 0x3FFF);

    CanFrame shortDlc = makeFrame(0x040, 3, {0, 0, 0x34});
    CHECK(!decodeEngineRpm(shortDlc, rpm));
}

void testAccelerator() {
    // data[4] = 255 -> 100% (clamped from 255/2.55 = 100.0 exactly, but
    // guards against float overrun regardless).
    CanFrame f = makeFrame(0x040, 5, {0, 0, 0, 0, 255});
    float pct = -1.0f;
    CHECK(decodeAcceleratorPercent(f, pct));
    CHECK_NEAR(pct, 100.0f, 0.01);

    CanFrame zero = makeFrame(0x040, 5, {0, 0, 0, 0, 0});
    CHECK(decodeAcceleratorPercent(zero, pct));
    CHECK_NEAR(pct, 0.0f, 0.01);

    CanFrame shortDlc = makeFrame(0x040, 4, {0, 0, 0, 0});
    CHECK(!decodeAcceleratorPercent(shortDlc, pct));
}

void testOilTemperature() {
    // data[3] = 140 -> 100C (brief's confirmed example).
    CanFrame f = makeFrame(0x345, 5, {0, 0, 0, 140, 0});
    float c = 0.0f;
    CHECK(decodeOilTemperatureC(f, c));
    CHECK_NEAR(c, 100.0f, 0.001);

    CanFrame shortDlc = makeFrame(0x345, 4, {0, 0, 0, 140});
    CHECK(!decodeOilTemperatureC(shortDlc, c));
}

void testCoolantTemperature() {
    // data[4] = 130 -> 90C (brief's confirmed example).
    CanFrame f = makeFrame(0x345, 5, {0, 0, 0, 0, 130});
    float c = 0.0f;
    CHECK(decodeCoolantTemperatureC(f, c));
    CHECK_NEAR(c, 90.0f, 0.001);
}

void testSteeringAngle() {
    // Positive raw value.
    int16_t rawPos = 100;
    CanFrame fPos = makeFrame(0x138, 4,
                               {0, 0, static_cast<uint8_t>(rawPos & 0xFF), static_cast<uint8_t>((rawPos >> 8) & 0xFF)});
    float deg = 0.0f;
    CHECK(decodeSteeringAngleDegrees(fPos, deg));
    CHECK_NEAR(deg, -10.0f, 0.001);  // raw * -0.1

    // Negative raw value (two's complement).
    int16_t rawNeg = -200;
    uint16_t rawNegBits = static_cast<uint16_t>(rawNeg);
    CanFrame fNeg = makeFrame(0x138, 4,
                              {0, 0, static_cast<uint8_t>(rawNegBits & 0xFF),
                               static_cast<uint8_t>((rawNegBits >> 8) & 0xFF)});
    CHECK(decodeSteeringAngleDegrees(fNeg, deg));
    CHECK_NEAR(deg, 20.0f, 0.001);

    CanFrame shortDlc = makeFrame(0x138, 3, {0, 0, 0});
    CHECK(!decodeSteeringAngleDegrees(shortDlc, deg));
}

void testLateralAcceleration() {
    // Positive int8.
    CanFrame fPos = makeFrame(0x13B, 7, {0, 0, 0, 0, 0, 0, 50});
    float scaled = 0.0f;
    CHECK(decodeLateralAcceleration(fPos, scaled));
    CHECK_NEAR(scaled, 10.0f, 0.001);  // 50 * 0.2

    // Negative int8 (two's complement byte 0xE2 = -30).
    CanFrame fNeg = makeFrame(0x13B, 7, {0, 0, 0, 0, 0, 0, 0xE2});
    CHECK(decodeLateralAcceleration(fNeg, scaled));
    CHECK_NEAR(scaled, -6.0f, 0.001);  // -30 * 0.2

    CanFrame shortDlc = makeFrame(0x13B, 6, {0, 0, 0, 0, 0, 0});
    CHECK(!decodeLateralAcceleration(shortDlc, scaled));
}

void testBrakePressure() {
    CanFrame f = makeFrame(0x139, 6, {0, 0, 0, 0, 0, 10});
    float scaled = 0.0f;
    CHECK(decodeBrakePressureScaled(f, scaled));
    CHECK_NEAR(scaled, 1280.0f, 0.001);  // 10 * 128

    CanFrame shortDlc = makeFrame(0x139, 5, {0, 0, 0, 0, 0});
    CHECK(!decodeBrakePressureScaled(shortDlc, scaled));
}

}  // namespace

int main() {
    testOilPressure();
    testEngineRpm();
    testAccelerator();
    testOilTemperature();
    testCoolantTemperature();
    testSteeringAngle();
    testLateralAcceleration();
    testBrakePressure();
    TEST_SUMMARY();
}
