#pragma once
#include <Arduino.h>
#include "shared_types.h"

// ============================================================
// storage_task — owns the SD card. Nothing else touches SD directly.
// Layer 3: mounts the board's built-in SDMMC slot and appends CSV
// rows to /health_log.csv. Falls back to Serial-only logging if the
// card fails to mount, so sensing/health/comms are unaffected either
// way — see storage_task.cpp for the fallback behavior.
// ============================================================

extern QueueHandle_t sensorQueue;
extern QueueHandle_t healthQueue;

void storageTask(void *pvParameters);