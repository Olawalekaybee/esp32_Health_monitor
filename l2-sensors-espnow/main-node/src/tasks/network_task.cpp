#include "network_task.h"
#include "config.h"

// Layer 1: deliberately offline-only stub, so the "online/offline"
// switch is architecturally proven before any Wi-Fi code exists.
// Layer 4 replaces the body with WiFi.begin()/status checks;
// Layer 5 adds the HTTPS POST to Google Sheets when connected.
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
