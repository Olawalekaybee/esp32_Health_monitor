#include "health_task.h"
#include "config.h"
#include "health_rules.h"

// This file is intentionally thin: all the actual decision logic
// lives in lib/health_rules (pure C++, unit-tested on host in CI).
// This wrapper only does the Arduino-side plumbing — reading the
// sensor queue, tracking failure streaks, and pushing the verdict
// onto the health queue.
static uint8_t dhtFailures = 0;
static uint8_t dsFailures  = 0;
static const uint8_t FAILURE_THRESHOLD = 3;

void healthTask(void *pvParameters) {
    SensorSample sample;

    for (;;) {
        HealthReport report;
        report.timestamp_ms = millis();

        if (xQueuePeek(sensorQueue, &sample, pdMS_TO_TICKS(200)) == pdTRUE) {
            sample.dht_read_ok    ? dhtFailures = 0 : dhtFailures++;
            sample.ds18b20_read_ok ? dsFailures = 0 : dsFailures++;

            HealthDecision decision = evaluate_health(dhtFailures, dsFailures, FAILURE_THRESHOLD);
            report.status = decision.status;
            strncpy(report.reason, decision.reason, sizeof(report.reason) - 1);
        } else {
            report.status = HealthStatus::UNKNOWN;
            snprintf(report.reason, sizeof(report.reason), "No sensor data yet");
        }

        xQueueOverwrite(healthQueue, &report);
        vTaskDelay(pdMS_TO_TICKS(HEALTH_CHECK_INTERVAL_MS));
    }
}
