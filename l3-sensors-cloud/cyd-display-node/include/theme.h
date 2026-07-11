#pragma once
#include <lvgl.h>
// ============================================================
// theme.h — every color and font the UI uses, named and in one
// place. Change the look here without touching ui.cpp's layout code.
// ============================================================

// --- Fonts ---
// Depend on which Montserrat variants lv_conf.h enabled — these fall
// back to the guaranteed-available default rather than assuming a
// specific size is compiled in, so this builds regardless of the
// exact lv_conf.h in use.
#if LV_FONT_MONTSERRAT_28
    #define FONT_TITLE &lv_font_montserrat_28
#else
    #define FONT_TITLE LV_FONT_DEFAULT
#endif
#if LV_FONT_MONTSERRAT_22
    #define FONT_VALUE &lv_font_montserrat_22
#else
    #define FONT_VALUE LV_FONT_DEFAULT
#endif
#if LV_FONT_MONTSERRAT_14
    #define FONT_LABEL &lv_font_montserrat_14
#else
    #define FONT_LABEL LV_FONT_DEFAULT
#endif

// --- Base palette (GitHub-dark inspired) ---
#define COLOR_BG           lv_color_hex(0x0b0f14)  // screen background
#define COLOR_TITLE_BAR    lv_color_hex(0x11161d)
#define COLOR_CARD_BG      lv_color_hex(0x161b22)
#define COLOR_CARD_BORDER  lv_color_hex(0x2a313c)
#define COLOR_BANNER_BG    lv_color_hex(0x1c2128)  // default/neutral banner
#define COLOR_TEXT_PRIMARY lv_color_hex(0xe6edf3)  // headings, values
#define COLOR_TEXT_MUTED   lv_color_hex(0x7d8590)  // labels, secondary text

// --- Status banner background per health state (muted, not
//     full-saturation, so the bright accent color still reads clearly
//     against it rather than fighting for attention) ---
#define COLOR_STATUS_BG_OK      lv_color_hex(0x1c3a24) // dark green
#define COLOR_STATUS_BG_WARNING lv_color_hex(0x3a3418) // dark amber
#define COLOR_STATUS_BG_FAULT   lv_color_hex(0x3a1c1c) // dark red
#define COLOR_STATUS_BG_UNKNOWN lv_color_hex(0x24272b) // dark grey

// --- Layout constants ---
#define TITLE_BAR_H   32
#define ACCENT_LINE_H 2
#define CARD_GAP      8
#define CARD_Y        (TITLE_BAR_H + ACCENT_LINE_H + 6)
#define CARD_H        112
#define CARD_W        148
#define BANNER_Y      (CARD_Y + CARD_H + CARD_GAP)
#define BANNER_H      (240 - BANNER_Y - 8)

// --- Sparkline trend graph (per-card temperature history) ---
#define SPARK_N       10   // how many past readings to show
#define SPARK_GAP     2
#define SPARK_MAX_H   22
#define SPARK_MIN_H   3
#define SPARK_W       (CARD_W - 20)  // content width inside card's 10px padding
#define SPARK_BAR_W   ((SPARK_W - (SPARK_N - 1) * SPARK_GAP) / SPARK_N)