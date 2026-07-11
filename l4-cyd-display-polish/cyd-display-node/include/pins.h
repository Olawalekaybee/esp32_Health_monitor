#pragma once
// ============================================================
// pins.h — single source of truth for this board's wiring.
// TFT SPI pins live in Setup_ESP32_2432S028R_ILI9341.h (TFT_eSPI's
// own config mechanism); everything else the app code touches
// directly goes here.
// ============================================================

// --- Touchscreen (XPT2046), currently unused (ENABLE_TOUCH 0 in
//     main.cpp) but defined here since it shares the SPI bus with
//     the display and its CS pin must be explicitly deselected
//     even while touch itself stays off. ---
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// --- Screen dimensions (portrait native resolution; layout code
//     assumes landscape after LV_DISPLAY_ROTATION_90 is applied) ---
#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 320

// --- I2C (slave mode, alternative transport to ESP-NOW) ---
// These are this board's own GPIO numbers — they do NOT need to
// match main-node's I2C pin numbers. What matters is that this
// board's SDA physically wires to main-node's SDA (same for SCL),
// regardless of which GPIO number each board uses for it.
#define I2C_SDA           27
#define I2C_SCL           22
#define I2C_SLAVE_ADDRESS 0x08