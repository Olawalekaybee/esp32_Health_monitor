#pragma once
// ============================================================
// pins.h — single source of truth for wiring.
//

// --- DHT22 (temperature + humidity, single-wire digital) ---
#define PIN_DHT22            1

// --- DS18B20 (Dallas temperature, OneWire bus) ---
#define PIN_DS18B20           2

// --- SD card (built-in slot, SDMMC 1-bit mode — NOT generic SPI) ---
// Confirmed against Keyestudio's official docs and profharris'
// GOOUUU_ESP32-S3-CAM repo: this board's SD slot is hardwired to
// these exact roles, not just "somewhere in GPIO38-40".
#define SD_CLK_PIN            39
#define SD_CMD_PIN            38
#define SD_D0_PIN             40

// --- Status LED ---
#define PIN_STATUS_LED       21

// --- I2C (optional transport to the CYD, alternative to ESP-NOW) ---
#define PIN_I2C_SDA          14
#define PIN_I2C_SCL          47
#define I2C_SLAVE_ADDRESS    0x08

// --- CYD display pins are board-specific and defined in the CYD
//     project's own TFT_eSPI setup — not duplicated here.