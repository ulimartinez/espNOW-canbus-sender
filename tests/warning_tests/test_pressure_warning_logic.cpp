#include "../../PressureWarningLogic.h"
#include "../test_framework.h"

namespace {

void testThresholdInterpolationEndpoints() {
    float caution, critical;

    interpolatePressureThresholds(0, caution, critical);  // below table -> clamp to first point
    CHECK_NEAR(caution, 10.0f, 0.001);
    CHECK_NEAR(critical, 6.0f, 0.001);

    interpolatePressureThresholds(9000, caution, critical);  // above table -> clamp to last point
    CHECK_NEAR(caution, 55.0f, 0.001);
    CHECK_NEAR(critical, 42.0f, 0.001);
}

void testThresholdInterpolationMidpoint() {
    float caution, critical;
    // Halfway between the 4000 {40,30} and 6000 {50,38} points.
    interpolatePressureThresholds(5000, caution, critical);
    CHECK_NEAR(caution, 45.0f, 0.001);
    CHECK_NEAR(critical, 34.0f, 0.001);
}

void testEngineOffNeverCritical() {
    // Zero pressure with the engine off must never report Critical --
    // that's the expected resting state, not a fault.
    PressureStatus status = evaluatePressureStatus(false, true, 0.0f, true, 0, false);
    CHECK(status == PressureStatus::Normal);
}

void testSensorErrorOverridesEverything() {
    PressureStatus status = evaluatePressureStatus(true, true, 50.0f, true, 3000, true);
    CHECK(status == PressureStatus::SensorError);
}

void testInvalidPressureIsUnknown() {
    PressureStatus status = evaluatePressureStatus(false, false, 0.0f, true, 3000, true);
    CHECK(status == PressureStatus::Unknown);
}

void testInvalidRpmIsUnknownWhenEngineRunning() {
    // Warning logic must not run without RPM once the engine is confirmed
    // running -- the RPM-dependent threshold table has no valid input.
    PressureStatus status = evaluatePressureStatus(false, true, 20.0f, false, 0, true);
    CHECK(status == PressureStatus::Unknown);
}

void testCautionAndCriticalAtHighRpm() {
    // At 6500 RPM: caution ~= 51.67 PSI, critical ~= 39.33 PSI (interpolated
    // between the 6000 and 7500 table points).
    PressureStatus normal = evaluatePressureStatus(false, true, 60.0f, true, 6500, true);
    CHECK(normal == PressureStatus::Normal);

    PressureStatus caution = evaluatePressureStatus(false, true, 45.0f, true, 6500, true);
    CHECK(caution == PressureStatus::Caution);

    PressureStatus critical = evaluatePressureStatus(false, true, 25.0f, true, 6500, true);
    CHECK(critical == PressureStatus::Critical);
}

}  // namespace

int main() {
    testThresholdInterpolationEndpoints();
    testThresholdInterpolationMidpoint();
    testEngineOffNeverCritical();
    testSensorErrorOverridesEverything();
    testInvalidPressureIsUnknown();
    testInvalidRpmIsUnknownWhenEngineRunning();
    testCautionAndCriticalAtHighRpm();
    TEST_SUMMARY();
}
