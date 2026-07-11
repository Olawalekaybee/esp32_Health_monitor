#pragma once
#include <Arduino.h>
#include "shared_types.h"

extern QueueHandle_t healthQueue;
void displayTask(void *pvParameters);
