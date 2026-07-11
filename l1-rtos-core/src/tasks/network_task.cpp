#include "network_task.h"
#include "config.h"

void networkTask(void *pvParameters) {
    for (;;) {
        if (ENABLE_CLOUD_SYNC) {
            Serial.println("[network_task] cloud sync enabled, but Wi-Fi not yet wired (Layer 4)");
        } else {
            Serial.println("[network_task] running in offline-only mode");
        }
        vTaskDelay(pdMS_TO_TICKS(CLOUD_SYNC_INTERVAL_MS));
    }
}
