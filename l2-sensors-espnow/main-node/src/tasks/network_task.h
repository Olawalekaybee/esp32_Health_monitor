#pragma once
#include <Arduino.h>
#include "shared_types.h"

// ============================================================
// network_task — owns Wi-Fi + cloud sync. This is the ONLY task
// allowed to know whether we're online or offline; everyone else
// just keeps working locally regardless.
// Layer 1 (now): stub — reports "offline" always, proves the pattern.
// Layer 4 (next): real Wi-Fi connect/reconnect state machine.
// Layer 5 (next): POST batched samples to Google Apps Script webhook.
// ============================================================

extern QueueHandle_t healthQueue;

void networkTask(void *pvParameters);
