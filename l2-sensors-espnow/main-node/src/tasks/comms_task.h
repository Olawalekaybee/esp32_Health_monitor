#pragma once
#include <Arduino.h>
#include "shared_types.h"

// ============================================================
// comms_task — owns the link(s) to the CYD display board. The CYD
// is a SEPARATE ESP32 board (with its own screen) — main-node and
// cyd-display-node are two independent firmwares. This task sends
// sensor + health data out and listens for acks coming back, over
// whichever transport(s) are enabled below.
//
// Transport selection: flip the USE_* defines in comms_task.cpp.
// At most 2 may be enabled at once (enforced at compile time).
// ============================================================

extern QueueHandle_t sensorQueue;
extern QueueHandle_t healthQueue;

void commsTask(void *pvParameters);
