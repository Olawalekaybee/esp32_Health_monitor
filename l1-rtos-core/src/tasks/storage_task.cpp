#include "storage_task.h"
#include "config.h"

void storageTask(void *pvParameters) {
    SensorSample sample;
    for (;;) {
        if (xQueuePeek(sensorQueue, &sample, pdMS_TO_TICKS(200)) == pdTRUE) {
            Serial.printf("[storage_task] would log: t=%lu value=%.3f ok=%d\n",
                          sample.timestamp_ms, sample.value, sample.read_ok);
        }
        vTaskDelay(pdMS_TO_TICKS(STORAGE_WRITE_INTERVAL_MS));
    }
}