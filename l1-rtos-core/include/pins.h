#pragma once
// pins.h — single source of truth for wiring.
#define PIN_SENSOR_ANALOG   1    // ADC1_CH0 on ESP32-S3 (GPIO34 was classic-ESP32-only, not valid here)
#define PIN_SD_CS           5
#define PIN_SD_MOSI         23
#define PIN_SD_MISO         19
#define PIN_SD_SCK          18
#define PIN_STATUS_LED      2