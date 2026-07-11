#include <unity.h>
#include "health_rules.h"

// These tests run on the host machine via `pio test -e native` —
// no ESP32 hardware required. This is what CI actually verifies
// for the health-decision logic, as opposed to just "does it compile."

void setUp(void) {}
void tearDown(void) {}

void test_both_sensors_healthy_is_OK(void) {
    HealthDecision d = evaluate_health(0, 0, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(HealthStatus::OK), static_cast<int>(d.status));
}

void test_one_recent_failure_is_WARNING_not_FAULT(void) {
    HealthDecision d = evaluate_health(1, 0, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(HealthStatus::WARNING), static_cast<int>(d.status));
}

void test_failures_below_threshold_is_WARNING(void) {
    HealthDecision d = evaluate_health(2, 0, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(HealthStatus::WARNING), static_cast<int>(d.status));
}

void test_dht_at_threshold_is_FAULT(void) {
    HealthDecision d = evaluate_health(3, 0, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(HealthStatus::FAULT), static_cast<int>(d.status));
}

void test_ds18b20_at_threshold_is_FAULT(void) {
    HealthDecision d = evaluate_health(0, 3, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(HealthStatus::FAULT), static_cast<int>(d.status));
}

void test_both_sensors_at_threshold_is_FAULT(void) {
    HealthDecision d = evaluate_health(5, 5, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(HealthStatus::FAULT), static_cast<int>(d.status));
}

// A failing sensor never masks a healthy one: this is the exact
// behavior a categorical "any sensor down = FAULT" rule would get
// right too, but a naive "average the sensors" rule would get wrong.
void test_dht_failure_does_not_mask_healthy_ds18b20_status_reason(void) {
    HealthDecision d = evaluate_health(3, 0, 3);
    TEST_ASSERT_EQUAL(static_cast<int>(HealthStatus::FAULT), static_cast<int>(d.status));
    TEST_ASSERT_TRUE(strstr(d.reason, "DHT22") != nullptr);
}

void test_reason_string_is_never_empty(void) {
    HealthDecision d1 = evaluate_health(0, 0, 3);
    HealthDecision d2 = evaluate_health(3, 3, 3);
    TEST_ASSERT_TRUE(strlen(d1.reason) > 0);
    TEST_ASSERT_TRUE(strlen(d2.reason) > 0);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_both_sensors_healthy_is_OK);
    RUN_TEST(test_one_recent_failure_is_WARNING_not_FAULT);
    RUN_TEST(test_failures_below_threshold_is_WARNING);
    RUN_TEST(test_dht_at_threshold_is_FAULT);
    RUN_TEST(test_ds18b20_at_threshold_is_FAULT);
    RUN_TEST(test_both_sensors_at_threshold_is_FAULT);
    RUN_TEST(test_dht_failure_does_not_mask_healthy_ds18b20_status_reason);
    RUN_TEST(test_reason_string_is_never_empty);
    return UNITY_END();
}
