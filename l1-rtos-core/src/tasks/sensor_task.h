#pragma once
#include <Arduino.h>
#include "shared_types.h"

extern QueueHandle_t sensorQueue;
void sensorTask(void *pvParameters);
