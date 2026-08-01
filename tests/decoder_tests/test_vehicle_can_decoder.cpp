#include "../../SignalConfig.h"
#include "../../VehicleCanDecoder.h"
#include "../test_framework.h"

namespace {

CanFrame makeOilPressureFrame(uint8_t psi) {
    CanFrame f;
    f.identifier = 0x171;
    f.data_length_code = 8;
    f.data[7] = psi;
    return f;
}

CanFrame makeRpmFrame(uint16_t rpm) {
    CanFrame f;
    f.identifier = 0x040;
    f.data_length_code = 5;
    f.data[2] = rpm & 0xFF;
    f.data[3] = (rpm >> 8) & 0x3F;
    f.data[4] = 0;
    return f;
}

void testImplausibleOilPressureSetsSensorError() {
    VehicleCanDecoder decoder;
    // 8-bit byte can't literally exceed 160 (MAX_VALID_OIL_PRESSURE_PSI),
    // but the plausibility check is exercised directly via a value the
    // decode function would still accept structurally (any 0-255 byte) --
    // 255 PSI is above the configured max.
    CHECK(decoder.processFrame(makeOilPressureFrame(255), 1000));
    CHECK(!decoder.data().oilPressureValid);
    CHECK(decoder.data().sensorError);
    CHECK_EQ(decoder.data().invalidOilPressureCount, 1u);
}

void testValidOilPressureClearsSensorError() {
    VehicleCanDecoder decoder;
    decoder.processFrame(makeOilPressureFrame(255), 1000);
    CHECK(decoder.data().sensorError);

    decoder.processFrame(makeOilPressureFrame(72), 1010);
    CHECK(decoder.data().oilPressureValid);
    CHECK_NEAR(decoder.data().oilPressurePsi, 72.0f, 0.001);
    CHECK(!decoder.data().sensorError);
}

void testMalformedFrameCounter() {
    VehicleCanDecoder decoder;
    // Known ID but insufficient DLC -> counted as malformed, not applied.
    CanFrame shortFrame;
    shortFrame.identifier = 0x171;
    shortFrame.data_length_code = 4;
    CHECK(!decoder.processFrame(shortFrame, 1000));
    CHECK_EQ(decoder.data().malformedFrameCount, 1u);
    CHECK(!decoder.data().oilPressureValid);
}

void testUninterestingIdIgnored() {
    VehicleCanDecoder decoder;
    CanFrame other;
    other.identifier = 0x999;
    other.data_length_code = 8;
    CHECK(!decoder.processFrame(other, 1000));
    CHECK_EQ(decoder.data().malformedFrameCount, 0u);  // not one of our IDs, not "malformed"
}

void testFreshnessExpiresStaleSignal() {
    VehicleCanDecoder decoder;
    decoder.processFrame(makeOilPressureFrame(72), 1000);
    CHECK(decoder.data().oilPressureValid);

    decoder.updateFreshness(1000 + OIL_PRESSURE_LOST_MS - 1);
    CHECK(decoder.data().oilPressureValid);  // not lost yet

    decoder.updateFreshness(1000 + OIL_PRESSURE_LOST_MS + 1);
    CHECK(!decoder.data().oilPressureValid);  // lost
}

void testEngineRunningHysteresisDebounce() {
    VehicleCanDecoder decoder;
    uint32_t t = 0;
    decoder.processFrame(makeRpmFrame(0), t);
    CHECK(!decoder.data().engineRunning);

    // Crossing the running threshold must persist for ENGINE_STATE_DEBOUNCE_MS
    // before engineRunning flips -- a single frame isn't enough.
    t += 10;
    decoder.processFrame(makeRpmFrame(1000), t);
    CHECK(!decoder.data().engineRunning);

    t += ENGINE_STATE_DEBOUNCE_MS + 10;
    decoder.processFrame(makeRpmFrame(1000), t);
    CHECK(decoder.data().engineRunning);

    // Dropping into the hysteresis dead zone (200-400 RPM) must not
    // immediately clear engineRunning.
    t += 10;
    decoder.processFrame(makeRpmFrame(300), t);
    CHECK(decoder.data().engineRunning);

    // Dropping below the stopped threshold, once debounced, clears it.
    t += 10;
    decoder.processFrame(makeRpmFrame(50), t);
    CHECK(decoder.data().engineRunning);  // not yet debounced
    t += ENGINE_STATE_DEBOUNCE_MS + 10;
    decoder.processFrame(makeRpmFrame(50), t);
    CHECK(!decoder.data().engineRunning);
}

void testProvisionalEngineRunningFromPressureWhenRpmInvalid() {
    VehicleCanDecoder decoder;
    uint32_t t = 0;
    decoder.processFrame(makeOilPressureFrame(50), t);  // rpmValid stays false
    CHECK(!decoder.data().rpmValid);
    CHECK(!decoder.data().engineRunning);

    t += PROVISIONAL_ENGINE_RUNNING_DEBOUNCE_MS + 10;
    decoder.processFrame(makeOilPressureFrame(50), t);
    CHECK(decoder.data().engineRunning);
    CHECK(decoder.data().engineRunningProvisional);
}

}  // namespace

int main() {
    testImplausibleOilPressureSetsSensorError();
    testValidOilPressureClearsSensorError();
    testMalformedFrameCounter();
    testUninterestingIdIgnored();
    testFreshnessExpiresStaleSignal();
    testEngineRunningHysteresisDebounce();
    testProvisionalEngineRunningFromPressureWhenRpmInvalid();
    TEST_SUMMARY();
}
