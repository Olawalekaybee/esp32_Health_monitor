#include "cloud/cloud_google_sheets.h"
#include "cloud/cloud_backend_select.h"

#if CLOUD_BACKEND_GOOGLE_SHEETS

#include "config.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static const char *statusToStr(HealthStatus s)
{
    switch (s)
    {
        case HealthStatus::OK:
            return "OK";

        case HealthStatus::WARNING:
            return "WARNING";

        case HealthStatus::FAULT:
            return "FAULT";

        default:
            return "UNKNOWN";
    }
}

// %.2f of a NAN prints the literal text "nan" with no quotes around it,
// which is not valid JSON (JSON has no NaN token) — JSON.parse() on the
// receiving end throws on it. Format failed readings as the JSON literal
// null instead, since sample.*_c is only ever NAN when the matching
// *_read_ok flag is false.
static void formatNumberOrNull(char *out, size_t outSize, float value, bool ok)
{
    if (ok)
    {
        snprintf(out, outSize, "%.2f", value);
    }
    else
    {
        snprintf(out, outSize, "null");
    }
}

static void buildJsonPayload(char *out,
                             size_t outSize,
                             const SensorSample &sample,
                             const HealthReport &health)
{
    char dhtTempStr[16];
    char dhtHumidityStr[16];
    char ds18b20TempStr[16];

    formatNumberOrNull(dhtTempStr, sizeof(dhtTempStr), sample.dht_temperature_c, sample.dht_read_ok);
    formatNumberOrNull(dhtHumidityStr, sizeof(dhtHumidityStr), sample.dht_humidity_pct, sample.dht_read_ok);
    formatNumberOrNull(ds18b20TempStr, sizeof(ds18b20TempStr), sample.ds18b20_temperature_c, sample.ds18b20_read_ok);

    snprintf(
        out,
        outSize,

        "{"
        "\"timestamp_ms\":%lu,"
        "\"dht_temp_c\":%s,"
        "\"dht_humidity_pct\":%s,"
        "\"dht_ok\":%d,"
        "\"ds18b20_temp_c\":%s,"
        "\"ds18b20_ok\":%d,"
        "\"health_status\":\"%s\","
        "\"health_reason\":\"%s\""
        "}",

        sample.timestamp_ms,
        dhtTempStr,
        dhtHumidityStr,
        sample.dht_read_ok,
        ds18b20TempStr,
        sample.ds18b20_read_ok,
        statusToStr(health.status),
        health.reason);
}

bool cloud_sync_init()
{
    Serial.println("[cloud_google_sheets] Initialized");
    return true;
}

void cloud_sync_send(const SensorSample &sample,
                     const HealthReport &health)
{
    char json[512];

    buildJsonPayload(json,
                     sizeof(json),
                     sample,
                     health);

    Serial.println();
    Serial.println("========================================");
    Serial.println("[Google Sheets] Sending JSON:");
    Serial.println(json);
    Serial.println("========================================");

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;

    if (!http.begin(client, CLOUD_ENDPOINT_URL))
    {
        Serial.println("[Google Sheets] ERROR: http.begin() failed");
        return;
    }

    http.addHeader("Content-Type", "application/json");

    int httpCode = http.POST((uint8_t *)json, strlen(json));

    Serial.printf("[Google Sheets] HTTP Code = %d\n", httpCode);

    String response = http.getString();

    Serial.println("[Google Sheets] Server Response:");
    Serial.println(response);

    http.end();
}

#endif