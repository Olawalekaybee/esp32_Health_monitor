#include "network_task.h"
#include "config.h"
#include <WiFi.h>
#include "cloud/cloud_dispatch.h"

static bool wifiConnected = false;
static CloudBackendId currentBackend = DEFAULT_CLOUD_BACKEND;

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
    status.active_cloud_backend = static_cast<uint8_t>(currentBackend);
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

// Checks statusQueue for a backend switch request from the CYD's
// settings screen. Only acts if it's both different from what's
// currently running and a genuinely valid enum value — comms_task
// relays whatever byte the CYD sent without validating it, so this is
// the one place that actually matters.
static void checkForBackendSwitch(const SystemStatus &status, bool &cloudReady, bool &lastCloudSyncOk) {
    if (!cloud_backend_id_valid(status.requested_cloud_backend)) {
        return;
    }

    CloudBackendId requested = static_cast<CloudBackendId>(status.requested_cloud_backend);
    if (requested == currentBackend) {
        return;
    }

    Serial.printf("[network_task] switching cloud backend: %s -> %s\n",
                  cloud_backend_name(currentBackend), cloud_backend_name(requested));
    currentBackend = requested;
    cloudReady = cloud_dispatch_init(currentBackend);
    lastCloudSyncOk = false; // haven't proven the new backend actually works yet
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
    bool cloudReady = cloud_dispatch_init(currentBackend);
    bool lastCloudSyncOk = false;

    for (;;) {
        SystemStatus status = {};
        xQueuePeek(statusQueue, &status, 0);
        checkForBackendSwitch(status, cloudReady, lastCloudSyncOk);

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
                    lastCloudSyncOk = cloud_dispatch_send(currentBackend, sample, health);
                }
            }
        }

        publishNetworkStatus(wifiConnected, lastCloudSyncOk);

        // Layer 6 (touch/settings): blocking on the semaphore instead
        // of a plain vTaskDelay means a force-sync tap on the CYD wakes
        // this loop immediately (xSemaphoreTake returns pdTRUE), while
        // an ordinary timeout (pdFALSE) falls through to the same
        // periodic behavior as before — either way the loop just runs
        // again from the top, no separate "do it now" code path needed.
        xSemaphoreTake(forceSyncSemaphore,
                       pdMS_TO_TICKS(wifiConnected ? CLOUD_SYNC_INTERVAL_MS : WIFI_RECONNECT_INTERVAL_MS));
    }
}
