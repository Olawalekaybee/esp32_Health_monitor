#include "display_task.h"
#include "config.h"

static const char* statusToStr(HealthStatus s) {
    switch (s) {
        case HealthStatus::OK:      return "OK";
        case HealthStatus::WARNING: return "WARNING";
        case HealthStatus::FAULT:   return "FAULT";
        default:                    return "UNKNOWN";
    }
}

void displayTask(void *pvParameters) {
    HealthReport report;
    for (;;) {
        if (xQueuePeek(healthQueue, &report, pdMS_TO_TICKS(200)) == pdTRUE) {
            Serial.printf("[display_task] would show: [%s] %s\n",
                          statusToStr(report.status), report.reason);
        }
        vTaskDelay(pdMS_TO_TICKS(DISPLAY_REFRESH_INTERVAL_MS));
    }
}
