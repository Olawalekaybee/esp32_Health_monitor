#pragma once
#include <Arduino.h>
#include "shared_types.h"

// ============================================================
// display_task — owns the screen. Layer 1 (now): stub, prints
// what WOULD be shown, to Serial — proves the health queue is
// readable without a screen attached yet.
// Layer 6 (next): swap Serial.printf for LVGL widget updates on CYD.
// ============================================================

extern QueueHandle_t healthQueue;

void displayTask(void *pvParameters);
