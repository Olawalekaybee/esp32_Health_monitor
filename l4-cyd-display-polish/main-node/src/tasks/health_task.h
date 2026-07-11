#pragma once
#include <Arduino.h>
#include "shared_types.h"

// ============================================================
// health_task — the "eps-claw" brain.
// Layer 1 (now): simple rule — did the last N reads succeed?
// Layer 9 (future): TinyML drift/anomaly detection locally,
//                    optional cloud AI call for deep diagnosis,
//                    WAMR module for hot-swappable rule sets.
// ============================================================

extern QueueHandle_t sensorQueue;
extern QueueHandle_t healthQueue;

void healthTask(void *pvParameters);