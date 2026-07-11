#include <Arduino.h>
#include "config.h"
#include "pins.h"
#include "shared_types.h"
#include "tasks/sensor_task.h"
#include "tasks/storage_task.h"
#include "tasks/network_task.h"
#include "tasks/display_task.h"
#include "tasks/health_task.h"

QueueHandle_t sensorQueue;
QueueHandle_t healthQueue;

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== ESP32 Health Monitor — Layer 1 boot ===");

    pinMode(PIN_STATUS_LED, OUTPUT);

    // Length 1, not a FIFO backlog: every consumer uses xQueuePeek to read
    // the latest sample without removing it, so this must behave as a
    // single-slot "mailbox" (see sensor_task.cpp's xQueueOverwrite).
    // A longer queue here would fill up and silently drop future samples,
    // since nothing ever calls xQueueReceive to drain it.
    sensorQueue = xQueueCreate(1, sizeof(SensorSample));
    healthQueue = xQueueCreate(1, sizeof(HealthReport));

    if (sensorQueue == NULL || healthQueue == NULL) {
        Serial.println("FATAL: queue creation failed");
        while (true) { delay(1000); }
    }

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

    Serial.println("All tasks started.");
}

void loop() {
    digitalWrite(PIN_STATUS_LED, !digitalRead(PIN_STATUS_LED));
    delay(1000);
}