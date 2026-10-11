#include "../../PressureWarningLogic.h"
#include "../../SignalConfig.h"
#include "../test_framework.h"

namespace {

void testThresholdInterpolationEndpoints() {
    float caution, critical;

    interpolatePressureThresholds(0, caution, critical);  // below table -> clamp to first point
    CHECK_NEAR(caution, 10.0f, 0.001);
    CHECK_NEAR(critical, 6.0f, 0.001);

    interpolatePressureThresholds(9000, caution, critical);  // above table -> clamp to last point
    CHECK_NEAR(caution, 42.0f, 0.001);
    CHECK_NEAR(critical, 32.0f, 0.001);
}

void testThresholdInterpolationMidpoint() {
    float caution, critical;
    // Halfway between the 6000 {32,20} and 6500 {28,18} table points.
    interpolatePressureThresholds(6250, caution, critical);
    CHECK_NEAR(caution, 30.0f, 0.001);
    CHECK_NEAR(critical, 19.0f, 0.001);
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
    // At 6500 RPM (an exact table point): caution=28 PSI, critical=18 PSI.
    PressureStatus normal = evaluatePressureStatus(false, true, 35.0f, true, 6500, true);
    CHECK(normal == PressureStatus::Normal);

    PressureStatus caution = evaluatePressureStatus(false, true, 23.0f, true, 6500, true);
    CHECK(caution == PressureStatus::Caution);

    PressureStatus critical = evaluatePressureStatus(false, true, 15.0f, true, 6500, true);
    CHECK(critical == PressureStatus::Critical);
}

// --- PressureWarningDebouncer ---
// See docs/can/pressure-threshold-derivation.md: real track logs show
// brief (<0.5s) pressure dips during normal hard cornering even on a
// healthy car, so escalation must require the condition to persist.

void testDebouncerDoesNotEscalateOnBriefDip() {
    PressureWarningDebouncer debouncer;
    uint32_t t = 0;

    // 15 PSI at 6500 RPM is Critical instantaneously (critical=18 there).
    CHECK(debouncer.evaluate(false, true, 15.0f, true, 6500, true, t) == PressureStatus::Normal);

    // Recovers well before PRESSURE_WARNING_DEBOUNCE_MS elapses -- must
    // never have escalated.
    t += PRESSURE_WARNING_DEBOUNCE_MS / 2;
    PressureStatus midway = debouncer.evaluate(false, true, 15.0f, true, 6500, true, t);
    CHECK(midway == PressureStatus::Normal);

    t += 5;
    CHECK(debouncer.evaluate(false, true, 60.0f, true, 6500, true, t) == PressureStatus::Normal);
}

void testDebouncerEscalatesOnSustainedDrop() {
    PressureWarningDebouncer debouncer;
    uint32_t t = 0;

    debouncer.evaluate(false, true, 15.0f, true, 6500, true, t);  // pending starts
    t += PRESSURE_WARNING_DEBOUNCE_MS - 1;
    CHECK(debouncer.evaluate(false, true, 15.0f, true, 6500, true, t) == PressureStatus::Normal);  // not yet

    t += 2;  // now >= PRESSURE_WARNING_DEBOUNCE_MS since the condition started
    CHECK(debouncer.evaluate(false, true, 15.0f, true, 6500, true, t) == PressureStatus::Critical);
}

void testDebouncerRecoveryIsInstant() {
    PressureWarningDebouncer debouncer;
    uint32_t t = 0;

    debouncer.evaluate(false, true, 15.0f, true, 6500, true, t);
    t += PRESSURE_WARNING_DEBOUNCE_MS + 1;
    CHECK(debouncer.evaluate(false, true, 15.0f, true, 6500, true, t) == PressureStatus::Critical);

    // Pressure recovers -- must clear on the very next sample, no debounce.
    t += 5;
    CHECK(debouncer.evaluate(false, true, 60.0f, true, 6500, true, t) == PressureStatus::Normal);
}

void testDebouncerBypassesForSensorErrorAndUnknown() {
    PressureWarningDebouncer debouncer;
    uint32_t t = 0;

    // SensorError must appear immediately, not wait out a debounce.
    CHECK(debouncer.evaluate(true, true, 15.0f, true, 6500, true, t) == PressureStatus::SensorError);

    // Same for Unknown (pressure signal lost).
    t += 5;
    CHECK(debouncer.evaluate(false, false, 0.0f, true, 6500, true, t) == PressureStatus::Unknown);
}

void testDebouncerRestartsPendingWindowOnDifferentSeverity() {
    PressureWarningDebouncer debouncer;
    uint32_t t = 0;

    // Starts pending Caution.
    debouncer.evaluate(false, true, 25.0f, true, 6500, true, t);  // caution=28,critical=18 at 6500
    t += PRESSURE_WARNING_DEBOUNCE_MS - 1;

    // Switches to pending Critical partway through -- must restart its own
    // window rather than inheriting the Caution pending time.
    CHECK(debouncer.evaluate(false, true, 15.0f, true, 6500, true, t) == PressureStatus::Normal);
    t += PRESSURE_WARNING_DEBOUNCE_MS - 1;
    CHECK(debouncer.evaluate(false, true, 15.0f, true, 6500, true, t) == PressureStatus::Normal);  // still not enough
    t += 2;
    CHECK(debouncer.evaluate(false, true, 15.0f, true, 6500, true, t) == PressureStatus::Critical);
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
    testDebouncerDoesNotEscalateOnBriefDip();
    testDebouncerEscalatesOnSustainedDrop();
    testDebouncerRecoveryIsInstant();
    testDebouncerBypassesForSensorErrorAndUnknown();
    testDebouncerRestartsPendingWindowOnDifferentSeverity();
    TEST_SUMMARY();
}
