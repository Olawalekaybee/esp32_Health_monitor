#include "network_task.h"
#include "config.h"
#include <WiFi.h>

// Backend selection lives in one place — see cloud/cloud_backend_select.h,
// which also guards each backend .cpp's implementation so unselected
// backends don't cause "multiple definition" linker errors.
#include "cloud/cloud_backend_select.h"

#if CLOUD_BACKEND_GOOGLE_SHEETS
    #include "cloud/cloud_google_sheets.h"
#elif CLOUD_BACKEND_FIREBASE
    #include "cloud/cloud_firebase.h"
#elif CLOUD_BACKEND_AWS
    #include "cloud/cloud_aws.h"
#endif

static bool wifiConnected = false;

// Read-modify-write on statusQueue: peek whatever's there (another
// task may have just updated a different field), overwrite only this
// task's own fields, push it back. Not a true atomic multi-field
// update — a genuinely simultaneous write from another task could
// still get clobbered — but these fields change at most once every
// few seconds/tens of seconds, so the actual collision window is tiny
// and the worst case is one stale field for one cycle, not a crash or
// permanently wrong value.
static void publishNetworkStatus(bool wifiOk, bool cloudOk) {
    SystemStatus status = {};
    xQueuePeek(statusQueue, &status, 0);
    status.wifi_connected = wifiOk;
    status.cloud_sync_ok = cloudOk;
    xQueueOverwrite(statusQueue, &status);
}

static bool attemptConnect() {
    Serial.printf("[network_task] connecting to Wi-Fi SSID '%s'...\n", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[network_task] Wi-Fi connected, IP: ");
        Serial.println(WiFi.localIP());
        return true;
    }

    Serial.println("[network_task] Wi-Fi connect attempt timed out — will retry later");
    return false;
}

void networkTask(void *pvParameters) {
    if (!ENABLE_CLOUD_SYNC) {
        // Still worth publishing "no Wi-Fi, no cloud" rather than
        // leaving statusQueue's seeded all-false values unexplained —
        // same values either way here, but this is the one place that
        // actually knows *why* they're false.
        for (;;) {
            Serial.println("[network_task] running in offline-only mode (ENABLE_CLOUD_SYNC is false)");
            publishNetworkStatus(false, false);
            vTaskDelay(pdMS_TO_TICKS(CLOUD_SYNC_INTERVAL_MS));
        }
    }

    wifiConnected = attemptConnect();
    bool cloudReady = cloud_sync_init();
    bool lastCloudSyncOk = false;

    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            if (wifiConnected) {
                Serial.println("[network_task] Wi-Fi connection lost");
                wifiConnected = false;
            }
            wifiConnected = attemptConnect();
            // No Wi-Fi this cycle means no sync attempt either — reflect
            // that immediately rather than showing a stale "last sync OK"
            // that's now meaningless with no connection.
            lastCloudSyncOk = false;
        } else {
            if (!wifiConnected) {
                Serial.println("[network_task] Wi-Fi reconnected");
            }
            wifiConnected = true;

            if (cloudReady) {
                SensorSample sample = {};
                HealthReport health = {};
                bool haveSample = (xQueuePeek(sensorQueue, &sample, pdMS_TO_TICKS(200)) == pdTRUE);
                bool haveHealth = (xQueuePeek(healthQueue, &health, 0) == pdTRUE);
                if (haveSample && haveHealth) {
                    lastCloudSyncOk = cloud_sync_send(sample, health);
                }
            }
        }

        publishNetworkStatus(wifiConnected, lastCloudSyncOk);

        vTaskDelay(pdMS_TO_TICKS(wifiConnected ? CLOUD_SYNC_INTERVAL_MS : WIFI_RECONNECT_INTERVAL_MS));
    }
}