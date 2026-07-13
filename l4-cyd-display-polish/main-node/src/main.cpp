#include <Arduino.h>
#include "config.h"
#include "pins.h"
#include "shared_types.h"
#include "tasks/sensor_task.h"
#include "tasks/storage_task.h"
#include "tasks/network_task.h"
#include "tasks/display_task.h"
#include "tasks/health_task.h"
#include "tasks/comms_task.h"

// ============================================================
// main.cpp — Layer 1: task wiring only. No business logic lives
// here on purpose. Each task file owns its own domain; this file
// just creates the queues (the contracts) and starts the tasks.
// ============================================================

QueueHandle_t sensorQueue;   // multiple readers -> use xQueuePeek, not xQueueReceive
QueueHandle_t healthQueue;   // length 1, always overwritten -> "latest verdict"
QueueHandle_t statusQueue;   // length 1, always overwritten -> "latest system status"
                             // (see shared_types.h's SystemStatus — wifi/SD/cloud/cyd-link,
                             // written by storage_task/network_task/comms_task, read by
                             // comms_task to build the outgoing packet to the CYD)

// Layer 6 (touch/settings): lets comms_task wake network_task
// immediately when the CYD's Cloud badge is tapped, instead of
// network_task only noticing on its next scheduled cycle (which could
// be up to CLOUD_SYNC_INTERVAL_MS away — using a polled status field
// for this would make "force sync now" not actually immediate). See
// network_task.cpp: it blocks on this semaphore with a timeout instead
// of a plain vTaskDelay, so a give() wakes it early while a timeout
// still falls through to the normal periodic behavior.
SemaphoreHandle_t forceSyncSemaphore;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 Health Monitor — Layer 1 boot ===");

    pinMode(PIN_STATUS_LED, OUTPUT);

    sensorQueue = xQueueCreate(1, sizeof(SensorSample)); // mailbox: all consumers use xQueuePeek, so this must be length-1 + overwrite, not a FIFO
    healthQueue = xQueueCreate(1, sizeof(HealthReport));
    statusQueue = xQueueCreate(1, sizeof(SystemStatus));
    forceSyncSemaphore = xSemaphoreCreateBinary();

    if (sensorQueue == NULL || healthQueue == NULL || statusQueue == NULL || forceSyncSemaphore == NULL) {
        Serial.println("FATAL: queue/semaphore creation failed");
        while (true) { delay(1000); }
    }

    // Seed with a known-safe "everything unknown/false" state so any
    // task that peeks before the owning task's first real update gets
    // a defined value, never garbage.
    SystemStatus initialStatus = {};
    initialStatus.overall_health = HealthStatus::UNKNOWN;
    xQueueOverwrite(statusQueue, &initialStatus);

    xTaskCreatePinnedToCore(sensorTask,  "sensor_task",  STACK_SENSOR_TASK,
                             NULL, PRIORITY_SENSOR_TASK, NULL, CORE_SENSOR_HEALTH);

    xTaskCreatePinnedToCore(healthTask,  "health_task",  STACK_HEALTH_TASK,
                             NULL, PRIORITY_HEALTH_TASK, NULL, CORE_SENSOR_HEALTH);

    xTaskCreatePinnedToCore(storageTask, "storage_task", STACK_STORAGE_TASK,
                             NULL, PRIORITY_STORAGE_TASK, NULL, CORE_SENSOR_HEALTH);

    xTaskCreatePinnedToCore(networkTask, "network_task", STACK_NETWORK_TASK,
                             NULL, PRIORITY_NETWORK_TASK, NULL, CORE_NETWORK_DISPLAY);

    xTaskCreatePinnedToCore(displayTask, "display_task", STACK_DISPLAY_TASK,
                             NULL, PRIORITY_DISPLAY_TASK, NULL, CORE_NETWORK_DISPLAY);

    xTaskCreatePinnedToCore(commsTask,   "comms_task",   STACK_COMMS_TASK,
                             NULL, PRIORITY_COMMS_TASK, NULL, CORE_NETWORK_DISPLAY);

    Serial.println("All tasks started.");
}

void loop() {
    // Intentionally empty — FreeRTOS tasks do all the work.
    // loop() itself runs as a low-priority task on core 1;
    // we just blink the status LED here as a heartbeat.
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
    delay(1000);
}
