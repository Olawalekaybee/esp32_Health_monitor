#include "health_task.h"
#include "config.h"

static uint8_t consecutiveFailures = 0;
static const uint8_t FAILURE_THRESHOLD = 3;

void healthTask(void *pvParameters) {
    SensorSample sample;
    for (;;) {
        HealthReport report;
        report.timestamp_ms = millis();
        if (xQueuePeek(sensorQueue, &sample, pdMS_TO_TICKS(200)) == pdTRUE) {
            if (!sample.read_ok) consecutiveFailures++;
            else consecutiveFailures = 0;

            if (consecutiveFailures >= FAILURE_THRESHOLD) {
                report.status = HealthStatus::FAULT;
                snprintf(report.reason, sizeof(report.reason),
                         "Sensor failed %d reads in a row", consecutiveFailures);
            } else if (consecutiveFailures > 0) {
                report.status = HealthStatus::WARNING;
                snprintf(report.reason, sizeof(report.reason),
                         "%d recent read failure(s)", consecutiveFailures);
            } else {
                report.status = HealthStatus::OK;
                snprintf(report.reason, sizeof(report.reason), "Nominal");
            }
        } else {
            report.status = HealthStatus::UNKNOWN;
            snprintf(report.reason, sizeof(report.reason), "No sensor data yet");
        }
        xQueueOverwrite(healthQueue, &report);
        vTaskDelay(pdMS_TO_TICKS(HEALTH_CHECK_INTERVAL_MS));
    }
}
