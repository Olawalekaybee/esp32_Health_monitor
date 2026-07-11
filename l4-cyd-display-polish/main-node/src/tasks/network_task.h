#pragma once
#include <Arduino.h>
#include "shared_types.h"

// ============================================================
// network_task — owns Wi-Fi connect/reconnect only. Cloud sync
// itself is delegated to whichever backend is selected in
// network_task.cpp (see include/cloud/).
// Layer 4 (was): real Wi-Fi connect/reconnect state machine.
// Layer 5 (now): actual cloud sync via a pluggable backend.
//
// Note: this coexists with ESP-NOW if ESP-NOW is one of your active
// comms_task transports, but they share the same radio — once this
// task connects to a real access point, the ESP32 locks to that AP's
// WiFi channel, and BOTH boards' ESP-NOW peers need to agree on that
// same channel or ESP-NOW silently stops delivering packets. Not an
// issue if you're currently on I2C/BLE instead.
// ============================================================

extern QueueHandle_t sensorQueue;
extern QueueHandle_t healthQueue;
extern QueueHandle_t statusQueue;

void networkTask(void *pvParameters);