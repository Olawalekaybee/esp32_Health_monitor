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
        for (;;) {
            Serial.println("[network_task] running in offline-only mode (ENABLE_CLOUD_SYNC is false)");
            vTaskDelay(pdMS_TO_TICKS(CLOUD_SYNC_INTERVAL_MS));
        }
    }

    wifiConnected = attemptConnect();
    bool cloudReady = cloud_sync_init();

    for (;;) {
        if (WiFi.status() != WL_CONNECTED) {
            if (wifiConnected) {
                Serial.println("[network_task] Wi-Fi connection lost");
                wifiConnected = false;
            }
            wifiConnected = attemptConnect();
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
                    cloud_sync_send(sample, health);
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(wifiConnected ? CLOUD_SYNC_INTERVAL_MS : WIFI_RECONNECT_INTERVAL_MS));
    }
}