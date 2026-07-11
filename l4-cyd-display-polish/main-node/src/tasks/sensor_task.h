#pragma once
#include <Arduino.h>
#include "shared_types.h"

// ============================================================
// sensor_task — owns both sensors. Nothing else touches these pins.
// Layer 2 (now): real drivers — DHT22 (temp+humidity) and DS18B20
//                (Dallas temp, OneWire). Independent read_ok flags
//                so a failure in one never hides the other's data.
// ============================================================

extern QueueHandle_t sensorQueue;   // defined in main.cpp
extern QueueHandle_t healthQueue;

void sensorTask(void *pvParameters);
