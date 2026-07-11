#pragma once
#define USER_SETUP_ID 303

#define ILI9341_2_DRIVER
#define TFT_RGB_ORDER TFT_BGR
#define TFT_INVERSION_ON

#define TFT_WIDTH   240
#define TFT_HEIGHT  320

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS   15
#define TFT_DC    2
#define TFT_RST  -1
#define TFT_BL   21
#define TFT_BACKLIGHT_ON HIGH

// Kept even though touch is currently disabled in main.cpp — this is
// needed once ENABLE_TOUCH is flipped back on for the CYD's touch layer.
#define TOUCH_CS 33

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       40000000
#define SPI_READ_FREQUENCY  20000000
#define SPI_TOUCH_FREQUENCY  1000000
