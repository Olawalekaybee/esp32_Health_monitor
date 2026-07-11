#pragma once
#include <Arduino.h>
#include "shared_types.h"

extern QueueHandle_t healthQueue;
void networkTask(void *pvParameters);
