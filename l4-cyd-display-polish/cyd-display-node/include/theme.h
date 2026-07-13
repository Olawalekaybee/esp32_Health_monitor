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
// Narrow side panels (WiFi/SD/Cloud/Comms badges) need a smaller face
// than the rest of the UI — 14pt wraps awkwardly in a ~36px column.
#if LV_FONT_MONTSERRAT_10
    #define FONT_SIDE_LABEL &lv_font_montserrat_10
#else
    #define FONT_SIDE_LABEL LV_FONT_DEFAULT
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
// Screen is 320x240 landscape, split into two vertical zones now: a
// single left side panel (WiFi/SD/Cloud/comms transport, all four
// stacked together) and a center zone (sensor cards + status banner)
// that gets the rest of the width. This replaces both the original
// full-width horizontal status strip and the later two-panel
// (left+right) version — consolidated to one side per request, which
// also means the cards get noticeably wider than before.
#define TITLE_BAR_H     32
#define ACCENT_LINE_H   2
#define PANEL_W         42   // left side panel width
#define CARD_GAP        6

#define CARD_Y          (TITLE_BAR_H + ACCENT_LINE_H + 6)
#define CARD_H          130  // taller than Layer 2/6 to fit the animated
                              // thermometer + humidity gauge added here
#define CARD_W          ((320 - PANEL_W - 3 * CARD_GAP) / 2)
#define BANNER_Y        (CARD_Y + CARD_H + CARD_GAP)
#define BANNER_H        (240 - BANNER_Y - 8)
#define CENTER_X         (PANEL_W + CARD_GAP)
#define CENTER_W         (320 - PANEL_W - 2 * CARD_GAP)

// --- Side panel: WiFi, SD, Cloud, and comms transport, all stacked in
//     one column. Vertical stacking/spacing is handled by LVGL's flex
//     layout in make_side_panel() (status_icons.cpp), not
//     hand-calculated Y offsets — PANEL_W itself is already defined
//     above. ---

// --- Vertical status ticker (status line + wrapped reason line,
//     scrolling as one unit inside the banner) — TICKER_CONTENT_H is
//     the single source of truth for its height, referenced from both
//     ui_create() and on_banner_clicked()'s resume logic, rather than
//     the same literal duplicated in two places (which is exactly how
//     a longer reason message ended up clipped: the value was too
//     small for 3 wrapped lines, and increasing it in only one of the
//     two spots would have silently reintroduced the same bug). ---
#define TICKER_CONTENT_H  80  // status line + up to 3 wrapped reason lines

// --- Sparkline trend graph — still used by nothing on the main
//     dashboard as of this layer (replaced by the thermometer/humidity
//     widgets), kept only because update_sparkline() itself is still a
//     generically useful helper if a future layer wants one back. ---
#define SPARK_N       10   // how many past readings to show
#define SPARK_GAP     2
#define SPARK_MAX_H   22
#define SPARK_MIN_H   3
#define SPARK_W       (CARD_W - 20)  // content width inside card's 10px padding
#define SPARK_BAR_W   ((SPARK_W - (SPARK_N - 1) * SPARK_GAP) / SPARK_N)