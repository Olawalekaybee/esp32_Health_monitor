#include "storage_task.h"
#include "config.h"
#include "pins.h"
#include <FS.h>
#include <SD_MMC.h>
#include <cstring>

// ============================================================
// Layer 3: real SD logging via the board's built-in SDMMC slot
// (1-bit mode — this board only wires CLK/CMD/D0, not D1-D3).
// If the card fails to mount (missing, unformatted, bad contacts),
// this falls back to the same Serial-only behavior Layer 1 had —
// sensing/health/comms keep running either way, since storage_task
// only ever reads from sensorQueue/healthQueue and never blocks
// anyone else.
//
// Logs BOTH the raw sensor readings AND the computed health verdict
// (status + reason) in the same row — otherwise the CSV can show
// the numbers but not *why* the system called something a fault,
// which defeats a lot of the point of keeping a log at all.
// ============================================================

static const char *LOG_PATH = "/health_log.csv";
static bool sdReady = false;

static const char *statusToStr(HealthStatus s) {
    switch (s) {
        case HealthStatus::OK:      return "OK";
        case HealthStatus::WARNING: return "WARNING";
        case HealthStatus::FAULT:   return "FAULT";
        default:                    return "UNKNOWN";
    }
}

static void writeHeaderIfNeeded() {
    if (!SD_MMC.exists(LOG_PATH)) {
        File f = SD_MMC.open(LOG_PATH, FILE_WRITE);
        if (f) {
            f.println("timestamp_ms,dht_temp_c,dht_humidity_pct,dht_ok,"
                       "ds18b20_temp_c,ds18b20_ok,health_status,health_reason");
            f.close();
        } else {
            Serial.println("[storage_task] WARNING: could not create log file header");
        }
    }
}

// CSV fields need commas/quotes escaped or the file becomes malformed
// the first time a health_reason happens to contain one. health_reason
// is currently built from fixed phrases + numbers (see health_rules),
// so a comma is unlikely but not impossible — cheap to guard against.
static void appendEscapedCsvField(File &f, const char *s) {
    bool needsQuotes = (strchr(s, ',') != nullptr) || (strchr(s, '"') != nullptr);
    if (!needsQuotes) {
        f.print(s);
        return;
    }
    f.print('"');
    for (const char *p = s; *p; p++) {
        if (*p == '"') f.print('"'); // double up embedded quotes
        f.print(*p);
    }
    f.print('"');
}

void storageTask(void *pvParameters) {
    if (!SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN)) {
        Serial.println("[storage_task] FATAL: SD_MMC.setPins failed — check pins.h matches your board");
    } else if (!SD_MMC.begin("/sdcard", true)) { // true = 1-bit mode (only D0 wired)
        Serial.println("[storage_task] WARNING: SD card mount failed — logging to Serial only. "
                        "Check a card is inserted and formatted FAT32.");
    } else {
        sdReady = true;
        Serial.printf("[storage_task] SD card mounted (%lluMB)\n",
                       (unsigned long long)(SD_MMC.cardSize() / (1024 * 1024)));
        writeHeaderIfNeeded();
    }

    SensorSample sample;
    HealthReport health;

    for (;;) {
        bool haveSample = (xQueuePeek(sensorQueue, &sample, pdMS_TO_TICKS(200)) == pdTRUE);
        bool haveHealth = (xQueuePeek(healthQueue, &health, 0) == pdTRUE);

        if (haveSample) {
            const char *statusStr = haveHealth ? statusToStr(health.status) : "UNKNOWN";
            const char *reasonStr = haveHealth ? health.reason : "";

            if (sdReady) {
                File f = SD_MMC.open(LOG_PATH, FILE_APPEND);
                if (f) {
                    f.printf("%lu,%.2f,%.2f,%d,%.2f,%d,",
                             sample.timestamp_ms,
                             sample.dht_temperature_c, sample.dht_humidity_pct, sample.dht_read_ok,
                             sample.ds18b20_temperature_c, sample.ds18b20_read_ok);
                    f.print(statusStr);
                    f.print(',');
                    appendEscapedCsvField(f, reasonStr);
                    f.println();
                    f.close();
                } else {
                    Serial.println("[storage_task] WARNING: failed to open log file for append");
                }
            } else {
                Serial.printf("[storage_task] (no SD) would log: t=%lu dht=%.1fC/%.0f%% (ok=%d) "
                               "ds18b20=%.2fC (ok=%d) status=%s reason=%s\n",
                               sample.timestamp_ms,
                               sample.dht_temperature_c, sample.dht_humidity_pct, sample.dht_read_ok,
                               sample.ds18b20_temperature_c, sample.ds18b20_read_ok,
                               statusStr, reasonStr);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(STORAGE_WRITE_INTERVAL_MS));
    }
}