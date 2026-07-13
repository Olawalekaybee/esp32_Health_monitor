#include "ui.h"
#include "theme.h"
#include "ui_common.h"
#include "status_icons.h"
#include <lvgl.h>
#include <Arduino.h>
#include <cstdio>
#include <cstring>
#include <cstdint>

// Built entirely from LVGL primitives (rounded rects + a circle), not
// an image asset — this hardware has no 3D renderer, so the "3D view"
// requested here means a stylized look (drop shadow, glow, smooth
// animated motion) rather than actual 3D geometry. Struct declared up
// here (rather than next to make_thermometer() below) since it needs
// to be a complete type before the global widget instances below it.

struct ThermoWidget {
    lv_obj_t *tube;
    lv_obj_t *mercury;
    lv_obj_t *bulb;
    int32_t last_fill_h;
};

// --- Widgets, created once in ui_create() ---
static lv_obj_t *dht_value_label;
static lv_obj_t *dht_sub_label;
static lv_obj_t *dht_dot;
static lv_obj_t *dht_spark_bars[SPARK_N];
static lv_obj_t *ds_value_label;
static lv_obj_t *ds_sub_label;
static lv_obj_t *ds_dot;
static lv_obj_t *ds_spark_bars[SPARK_N];

// Animated thermometer per sensor card, replacing the old sparkline
// bars — see ThermoWidget/make_thermometer below.
static ThermoWidget dht_thermo;
static ThermoWidget ds_thermo;

// DHT-only: humidity has no equivalent on the DS18B20 (single-value
// sensor), so this stays specific to the DHT card rather than a
// shared pair like the thermometers above.
static lv_obj_t *dht_humidity_arc;
static lv_obj_t *dht_humidity_value_label;
static int32_t dht_humidity_last_val = 0;

static lv_obj_t *status_banner;
static lv_obj_t *status_label;
static lv_obj_t *reason_label;
static lv_obj_t *ticker_content; // holds status_label+reason_label together; this is
                                  // what actually gets animated for the vertical scroll
static lv_obj_t *comms_dot;
static lv_obj_t *comms_label;
static lv_obj_t *dht_card_ref;
static lv_obj_t *ds_card_ref;

// Layer 6: system-status strip (WiFi/SD/Cloud), previously invisible
// on the CYD — you'd only have seen these in main-node's Serial log.
static lv_obj_t *wifi_dot;
static lv_obj_t *sd_dot;
static lv_obj_t *cloud_dot;

// Layer 6 (touch/settings): three screens total. main_screen is the
// existing dashboard (ui_create() still builds it first, unchanged).
// settings_screen and detail_screen are new, built at the end of
// ui_create(), and are otherwise idle/invisible until navigated to.
static lv_obj_t *main_screen;
static lv_obj_t *settings_screen;
static lv_obj_t *detail_screen;

static bool ticker_paused = false;

// Settings screen: one dot + one checkmark per backend option row.
// The dot mirrors the main dashboard's status-dot language; the
// checkmark is the clearer "this one is actually active" signal,
// shown/hidden rather than just recolored.
static lv_obj_t *settings_option_dot[3];
static lv_obj_t *settings_option_check[3];
static lv_obj_t *settings_active_label;

// Detail screen: a real LVGL arc gauge (not a styled plain object)
// plus a real LVGL chart for history — replacing the earlier
// hand-rolled bar widgets with LVGL's own widgets for both.
//
// Two different widget sets share this one screen, shown/hidden based
// on detail_which: DS18B20 (single value, no humidity) uses
// detail_arc + detail_chart below; DHT22 (has both temp AND humidity)
// uses the separate detail_dht_* pair instead, side by side, since a
// single arc was only ever showing temperature and never had anywhere
// to show humidity at all.
static lv_obj_t *detail_title_label;
static lv_obj_t *detail_value_label;
static lv_obj_t *detail_arc;
static lv_obj_t *detail_chart;
static lv_chart_series_t *detail_chart_series;

static lv_obj_t *detail_dht_temp_arc;
static lv_obj_t *detail_dht_temp_value_label;
static lv_obj_t *detail_dht_temp_caption;
static lv_obj_t *detail_dht_humidity_arc;
static lv_obj_t *detail_dht_humidity_value_label;
static lv_obj_t *detail_dht_humidity_caption;

static int detail_which = 0; // 0 = DHT22, 1 = DS18B20 — set when a card is tapped

// Rolling history for each card's sparkline, oldest first.
static float dht_temp_history[SPARK_N];
static float ds_temp_history[SPARK_N];
static bool history_initialized = false;

static const char *statusToStr(uint8_t s) {
    switch (s) {
        case 0: return "OK";
        case 1: return "WARNING";
        case 2: return "FAULT";
        default: return "UNKNOWN";
    }
}

// Layer 6 (touch/settings): mirrors main-node's CloudBackendId naming
// (see cloud_dispatch.h over there) — duplicated rather than shared,
// since these are two separate PlatformIO projects and this is just
// three strings, not worth a shared header for.
static const char *cloudBackendName(uint8_t id) {
    switch (id) {
        case 0: return "Google Sheets";
        case 1: return "Firebase";
        case 2: return "AWS";
        default: return "Unknown";
    }
}

static lv_color_t statusToColor(uint8_t s) {
    switch (s) {
        case 0: return lv_palette_main(LV_PALETTE_GREEN);
        case 1: return lv_color_hex(0xffa500); // true amber, not muted yellow
        case 2: return lv_palette_main(LV_PALETTE_RED);
        default: return lv_palette_main(LV_PALETTE_GREY);
    }
}

static lv_color_t statusToBgColor(uint8_t s) {
    switch (s) {
        case 0: return COLOR_STATUS_BG_OK;
        case 1: return lv_color_hex(0x3a2a10); // dark amber, matches the new true-amber accent
        case 2: return COLOR_STATUS_BG_FAULT;
        default: return COLOR_STATUS_BG_UNKNOWN;
    }
}

// Strip every style a freshly-created object might have inherited
// (theme, defaults) before applying our own from scratch. With the
// theme disabled globally (see main.cpp's lv_display_set_theme call)
// this is mostly a safety net, but keeps every widget's appearance
// fully explicit and predictable.
// bare(), make_status_dot(), set_dot_color() now live in ui_common.h/.cpp.
// make_side_panel()/make_status_icon_badge() now live in status_icons.h/.cpp,
// using real LV_SYMBOL_* icons instead of the wrapped text labels this
// used to have — see status_icons.h for why.

// ============================================================
// Animated "thermometer" — a vertical tube, a bulb, and an inner
// "mercury" fill whose height animates smoothly toward each new
// reading. Built entirely from LVGL primitives (rounded rects + a
// circle), not an image asset — this hardware has no 3D renderer, so
// "3D view" here means a stylized look (drop shadow, glow, smooth
// motion) rather than actual 3D geometry. Worth being upfront about
// that distinction rather than implying more than LVGL can do.
// (ThermoWidget struct itself is declared near the top of the file,
// alongside the global widget instances that need it as a complete type.)
// ============================================================

static void mercury_height_anim_cb(void *var, int32_t value) {
    lv_obj_t *mercury = (lv_obj_t *)var;
    lv_obj_set_height(mercury, value);
    // Re-anchor to the tube's bottom every frame so it visibly "fills
    // up" rather than growing down from a fixed top edge.
    lv_obj_align(mercury, LV_ALIGN_BOTTOM_MID, 0, -2);
}

static void animate_mercury_to(lv_obj_t *mercury, int32_t from, int32_t to, uint32_t duration_ms) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, mercury);
    lv_anim_set_exec_cb(&a, mercury_height_anim_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, duration_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static ThermoWidget make_thermometer(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                       lv_coord_t tube_w, lv_coord_t tube_h, lv_coord_t bulb_d) {
    ThermoWidget t = {};

    t.tube = lv_obj_create(parent);
    bare(t.tube);
    lv_obj_set_size(t.tube, tube_w, tube_h);
    lv_obj_set_pos(t.tube, x, y);
    lv_obj_set_style_radius(t.tube, tube_w / 2, 0);
    lv_obj_set_style_bg_color(t.tube, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_opa(t.tube, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t.tube, 1, 0);
    lv_obj_set_style_border_color(t.tube, COLOR_CARD_BORDER, 0);

    t.mercury = lv_obj_create(t.tube);
    bare(t.mercury);
    lv_coord_t mw = tube_w - 6;
    lv_obj_set_size(t.mercury, mw, 2);
    lv_obj_align(t.mercury, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_radius(t.mercury, mw / 2, 0);
    lv_obj_set_style_bg_color(t.mercury, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_bg_opa(t.mercury, LV_OPA_COVER, 0);

    t.bulb = lv_obj_create(parent);
    bare(t.bulb);
    lv_obj_set_size(t.bulb, bulb_d, bulb_d);
    lv_obj_set_pos(t.bulb, x + tube_w / 2 - bulb_d / 2, y + tube_h - bulb_d / 2 - 2);
    lv_obj_set_style_radius(t.bulb, bulb_d / 2, 0);
    lv_obj_set_style_bg_color(t.bulb, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_bg_opa(t.bulb, LV_OPA_COVER, 0);
    // Glow + gentle pulse for a bit of life and visual depth, reusing
    // the same shadow-pulse mechanic the status dots already use.
    lv_obj_set_style_shadow_width(t.bulb, 10, 0);
    lv_obj_set_style_shadow_color(t.bulb, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_shadow_opa(t.bulb, LV_OPA_40, 0);
    start_pulse(t.bulb, 1200);

    t.last_fill_h = 2;
    return t;
}

static void set_thermometer_value(ThermoWidget &t, float value, bool ok,
                                    float min_val, float max_val, lv_coord_t tube_h) {
    lv_color_t color = ok ? lv_palette_main(LV_PALETTE_CYAN) : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_bg_color(t.mercury, color, 0);
    lv_obj_set_style_bg_color(t.bulb, color, 0);
    lv_obj_set_style_shadow_color(t.bulb, color, 0);

    // A failed read has no real value to show — hold the mercury at
    // its floor rather than animating toward a NaN-derived height.
    int32_t target_h = 2;
    if (ok) {
        float clamped = value;
        if (clamped < min_val) clamped = min_val;
        if (clamped > max_val) clamped = max_val;
        float norm = (clamped - min_val) / (max_val - min_val);
        target_h = 2 + (int32_t)(norm * (tube_h - 8));
    }

    animate_mercury_to(t.mercury, t.last_fill_h, target_h, 400);
    t.last_fill_h = target_h;
}

// ============================================================
// Animated humidity gauge — a small circular arc (real LVGL widget,
// same pattern as the Detail screen's temperature arc) with the
// percentage centered inside it, and its value transition animated
// rather than snapping instantly.
// ============================================================
static void arc_value_anim_cb(void *var, int32_t value) {
    lv_arc_set_value((lv_obj_t *)var, value);
}

static void animate_arc_to(lv_obj_t *arc, int32_t from, int32_t to, uint32_t duration_ms) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, arc);
    lv_anim_set_exec_cb(&a, arc_value_anim_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, duration_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

// add_corner_brackets() now lives in ui_common.h/.cpp.

// --- Sparkline: a compact bar-style trend graph, built from plain
//     rectangles (not LVGL's lv_chart widget) so it doesn't depend on
//     LV_USE_CHART being enabled in lv_conf.h — this project has hit
//     enough "unverified LVGL feature" surprises already. Lives inside
//     a zero-padding container so child positions are unambiguous. ---

// Creates SPARK_N bar objects inside `card`, positioned in a row at
// (x, y) relative to the card's own top-left. Returns via `bars_out`.
static void create_sparkline(lv_obj_t *card, lv_coord_t x, lv_coord_t y, lv_obj_t **bars_out) {
    lv_obj_t *container = lv_obj_create(card);
    bare(container);
    lv_obj_set_size(container, SPARK_W, SPARK_MAX_H);
    lv_obj_set_pos(container, x, y);

    for (int i = 0; i < SPARK_N; i++) {
        lv_obj_t *bar = lv_obj_create(container);
        bare(bar);
        lv_obj_set_size(bar, SPARK_BAR_W, SPARK_MIN_H);
        lv_obj_set_pos(bar, i * (SPARK_BAR_W + SPARK_GAP), SPARK_MAX_H - SPARK_MIN_H);
        lv_obj_set_style_bg_color(bar, lv_color_hex(0x2a313c), 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(bar, 1, 0);
        bars_out[i] = bar;
    }
}

// Recomputes every bar's height from a history array (oldest first),
// scaled between min_val/max_val, colored by `color`.
static void update_sparkline(lv_obj_t **bars, const float *history,
                              float min_val, float max_val, lv_color_t color) {
    float range = max_val - min_val;
    if (range < 0.01f) range = 0.01f; // avoid divide-by-zero if flat

    for (int i = 0; i < SPARK_N; i++) {
        float v = history[i];
        if (v < min_val) v = min_val;
        if (v > max_val) v = max_val;
        float norm = (v - min_val) / range;
        lv_coord_t h = SPARK_MIN_H + (lv_coord_t)(norm * (SPARK_MAX_H - SPARK_MIN_H));
        lv_obj_set_size(bars[i], SPARK_BAR_W, h);
        lv_obj_set_pos(bars[i], i * (SPARK_BAR_W + SPARK_GAP), SPARK_MAX_H - h);
        lv_obj_set_style_bg_color(bars[i], color, 0);
    }
}

// Shifts a history buffer left by one and appends `value` at the end.
static void push_history(float *history, float value) {
    memmove(history, history + 1, (SPARK_N - 1) * sizeof(float));
    history[SPARK_N - 1] = value;
}

// add_press_feedback() now lives in ui_common.h/.cpp.

// Shared header for the Settings and Detail screens: the ENTIRE bar
// is the back-tap target, not a small corner button — deliberately
// generous (320x32 = ~5x the area of the original 60x32 corner
// button) so touch accuracy near a screen edge can't be the reason it
// doesn't register, whatever the original cause turns out to be.
// out_title_label is optional — Detail screen needs to update its
// title text per-sensor later; Settings screen's title never changes.
static lv_obj_t *make_nav_header(lv_obj_t *parent, const char *title,
                                   lv_event_cb_t back_cb, lv_obj_t **out_title_label = nullptr) {
    lv_obj_t *header = lv_obj_create(parent);
    bare(header);
    lv_obj_set_size(header, 320, TITLE_BAR_H);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_bg_color(header, COLOR_TITLE_BAR, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
    // Same reasoning as the gear button's extended click area: this
    // sits right at the top edge, the least accurate zone on a
    // resistive panel. Already full-width, so the only useful
    // direction to extend is downward, giving a slightly-low tap a
    // little more room to still register as "back" instead of
    // falling through to whatever's just below the header.
    lv_obj_set_ext_click_area(header, 12);
    lv_obj_add_event_cb(header, back_cb, LV_EVENT_CLICKED, NULL);
    add_press_feedback(header, lv_color_hex(0x232a35));

    lv_obj_t *back_label = lv_label_create(header);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_font(back_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(back_label, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);
    lv_obj_align(back_label, LV_ALIGN_LEFT_MID, 10, 0);

    lv_obj_t *title_label = lv_label_create(header);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_font(title_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_letter_space(title_label, 1, 0);
    lv_obj_align(title_label, LV_ALIGN_RIGHT_MID, -10, 0);

    if (out_title_label) {
        *out_title_label = title_label;
    }
    return header;
}

// start_pulse(), start_accent_glow(), and flash_border() now live in
// ui_common.h/.cpp.

// Vertical ticker scroll: moves `obj` (the combined status+reason
// container) from fully below the viewport up to fully above it, then
// snaps back and repeats — a vertical version of the horizontal
// scrolling ticker used elsewhere in this file, since a fixed-height
// banner can't otherwise show a status line and a wrapped reason line
// at once without one getting clipped.
static void vertical_scroll_anim_cb(void *var, int32_t value) {
    lv_obj_set_y((lv_obj_t *)var, value);
}

static void start_vertical_ticker(lv_obj_t *obj, lv_coord_t start_y, lv_coord_t end_y, uint32_t duration_ms) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, vertical_scroll_anim_cb);
    lv_anim_set_values(&a, start_y, end_y);
    lv_anim_set_time(&a, duration_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_linear); // constant speed, not ease — reads as a steady crawl, not a bounce
    lv_anim_start(&a);
}

// ============================================================
// Layer 6 (touch/settings): touch event handlers. All of LVGL's
// input handling funnels through lv_event_cb_t callbacks like these,
// registered with lv_obj_add_event_cb() in ui_create() below.
// ============================================================

static void on_gear_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    Serial.println("[touch] gear tapped -> settings screen");
    lv_screen_load(settings_screen);
}

static void on_settings_back_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    Serial.println("[touch] settings back tapped -> main screen");
    lv_screen_load(main_screen);
}

// The tapped backend id (0/1/2) is passed as this callback's
// user_data when the button is created — see the settings screen
// build-out below.
static void on_backend_option_clicked(lv_event_t *e) {
    uint8_t backend_id = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    Serial.printf("[touch] backend option %d tapped\n", backend_id);
    app_request_cloud_backend(backend_id);
    // Deliberately not updating settings_option_dot[] here — that's
    // driven by pkt.active_cloud_backend in ui_update(), so the
    // highlighted option always reflects what main-node has actually
    // confirmed, not just what was last tapped. If main-node is slow
    // to switch, the tapped option's dot simply won't light up yet —
    // an honest "pending" state rather than a fake instant one.
}

static void on_cloud_badge_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    Serial.println("[touch] cloud badge tapped -> force sync");
    app_request_force_sync();
}

// Pausing is implemented as delete-the-running-animation (which
// freezes the object at its current position, a standard LVGL
// pattern) rather than a dedicated pause API. Resuming starts a fresh
// animation from wherever it currently sits — always a fixed 7s
// regardless of remaining distance, a deliberate simplification over
// computing a proportional duration, since the visual difference
// (slightly faster or slower resume) is minor and not worth the
// extra complexity here.
static void on_banner_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    ticker_paused = !ticker_paused;
    Serial.printf("[touch] banner tapped -> ticker %s\n", ticker_paused ? "paused" : "resumed");

    if (ticker_paused) {
        lv_anim_del(ticker_content, vertical_scroll_anim_cb);
    } else {
        start_vertical_ticker(ticker_content, lv_obj_get_y(ticker_content), -TICKER_CONTENT_H, 7000);
    }
}

static void on_dht_card_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    Serial.println("[touch] DHT card tapped -> detail screen");
    detail_which = 0;
    lv_label_set_text(detail_title_label, "DHT22 Detail");
    lv_obj_clear_flag(detail_dht_temp_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_dht_temp_caption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_dht_humidity_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_dht_humidity_caption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_chart, LV_OBJ_FLAG_HIDDEN);
    lv_screen_load(detail_screen);
}

static void on_ds_card_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    Serial.println("[touch] DS18B20 card tapped -> detail screen");
    detail_which = 1;
    lv_label_set_text(detail_title_label, "DS18B20 Detail");
    lv_obj_add_flag(detail_dht_temp_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_dht_temp_caption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_dht_humidity_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_dht_humidity_caption, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(detail_chart, LV_OBJ_FLAG_HIDDEN);
    lv_screen_load(detail_screen);
}

static void on_detail_back_clicked(lv_event_t *e) {
    LV_UNUSED(e);
    Serial.println("[touch] detail back tapped -> main screen");
    lv_screen_load(main_screen);
}

void ui_create(void) {
    lv_obj_t *screen = lv_screen_active();
    main_screen = screen; // Layer 6: named so on_*_back_clicked() handlers can navigate back to it
    bare(screen);
    lv_obj_set_style_bg_color(screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    // --- Title bar with a thin accent line underneath ---
    lv_obj_t *title_bar = lv_obj_create(screen);
    bare(title_bar);
    lv_obj_set_size(title_bar, 320, TITLE_BAR_H);
    lv_obj_set_pos(title_bar, 0, 0);
    lv_obj_set_style_bg_color(title_bar, COLOR_TITLE_BAR, 0);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);

    lv_obj_t *title_label = lv_label_create(title_bar);
    lv_label_set_text(title_label, "ESP32 HEALTH MONITOR");
    lv_obj_set_style_text_font(title_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(title_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_letter_space(title_label, 1, 0);
    lv_obj_align(title_label, LV_ALIGN_LEFT_MID, 8, 0);

    // --- Settings gear: opens the cloud-backend settings screen.
    //     Wrapped in its own 28px-wide clickable container rather than
    //     making just the glyph tappable — a bare label's hit-box is
    //     only as big as the rendered characters, too small and fiddly
    //     to reliably tap on a real touchscreen. ---
    lv_obj_t *gear_btn = lv_obj_create(title_bar);
    bare(gear_btn);
    lv_obj_set_size(gear_btn, 28, TITLE_BAR_H);
    lv_obj_align(gear_btn, LV_ALIGN_RIGHT_MID, -6, 0); // comms badge moved out of the title bar (now in the right side panel), so this no longer needs to share space with it
    lv_obj_set_style_bg_opa(gear_btn, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(gear_btn, LV_OBJ_FLAG_CLICKABLE);
    // Extends the CLICKABLE area beyond what's visually drawn — this
    // sits right at the top-right corner, the single least accurate
    // spot on a resistive panel (same edge-effect that motivated the
    // touch debounce fix). A generous extra margin here means a
    // slightly-off tap still lands on the gear icon instead of
    // missing it and falling through to whatever's near it.
    lv_obj_set_ext_click_area(gear_btn, 16);
    add_press_feedback(gear_btn, lv_color_hex(0x232a35));
    lv_obj_add_event_cb(gear_btn, on_gear_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t *gear_label = lv_label_create(gear_btn);
    lv_label_set_text(gear_label, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(gear_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(gear_label, COLOR_TEXT_MUTED, 0);
    lv_obj_center(gear_label);

    lv_obj_t *accent_line = lv_obj_create(screen);
    bare(accent_line);
    lv_obj_set_size(accent_line, 320, ACCENT_LINE_H);
    lv_obj_set_pos(accent_line, 0, TITLE_BAR_H);
    lv_obj_set_style_bg_opa(accent_line, LV_OPA_COVER, 0);
    start_accent_glow(accent_line);

    // --- Left side panel: WiFi, SD, Cloud, and comms transport — all
    //     four together in one column now, per request. A flex column
    //     container handles the vertical stacking and spacing itself;
    //     nothing here is manually positioned, which is deliberate
    //     after a hand-calculated version of this rendered a badge far
    //     bigger and in the wrong place than intended. Each badge
    //     already pulses internally (see make_status_icon_badge()), so
    //     no separate start_pulse() call is needed here. ---
    lv_coord_t panel_h = 240 - TITLE_BAR_H - ACCENT_LINE_H;
    lv_obj_t *left_panel = make_side_panel(screen, 0, panel_h);

    wifi_dot = make_status_icon_badge(left_panel, LV_SYMBOL_WIFI);
    sd_dot = make_status_icon_badge(left_panel, LV_SYMBOL_SD_CARD);

    lv_obj_t *cloud_badge_container = nullptr;
    cloud_dot = make_status_icon_badge(left_panel, LV_SYMBOL_UPLOAD, &cloud_badge_container);
    lv_obj_add_flag(cloud_badge_container, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(cloud_badge_container, 10); // sits at the screen's left edge — same resistive-panel edge-accuracy concern as the gear button
    add_press_feedback(cloud_badge_container, lv_color_hex(0x232a35));
    lv_obj_add_event_cb(cloud_badge_container, on_cloud_badge_clicked, LV_EVENT_CLICKED, NULL);

    // No dedicated symbol fits "whichever transport happens to be
    // active" (I2C/ESP-NOW/Bluetooth) — LV_SYMBOL_REFRESH stands in
    // generically for "data exchange" regardless of which one is
    // compiled in. comms_label is no longer a visible text label
    // (icon-only badges now), but ui_set_comms_status() still accepts
    // the transport name in case a future layer wants it back.
    comms_dot = make_status_icon_badge(left_panel, LV_SYMBOL_REFRESH);

    // --- Sensor cards ---
    lv_obj_t *dht_card = lv_obj_create(screen);
    dht_card_ref = dht_card;
    bare(dht_card);
    lv_obj_set_size(dht_card, CARD_W, CARD_H);
    lv_obj_set_pos(dht_card, CENTER_X, CARD_Y);
    lv_obj_set_style_bg_color(dht_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(dht_card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(dht_card, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_grad_dir(dht_card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(dht_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(dht_card, 1, 0);
    lv_obj_set_style_radius(dht_card, 2, 0);
    lv_obj_set_style_pad_all(dht_card, 10, 0);
    add_corner_brackets(screen, CENTER_X, CARD_Y, CARD_W, CARD_H, lv_palette_main(LV_PALETTE_CYAN));
    lv_obj_add_flag(dht_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(dht_card, 6);
    add_press_feedback(dht_card, lv_color_hex(0x1c2430));
    lv_obj_add_event_cb(dht_card, on_dht_card_clicked, LV_EVENT_CLICKED, NULL);

    dht_dot = make_status_dot(dht_card);
    lv_obj_align(dht_dot, LV_ALIGN_TOP_RIGHT, 0, 2);
    start_pulse(dht_dot, 900);

    lv_obj_t *dht_header = lv_label_create(dht_card);
    lv_label_set_text(dht_header, "DHT22");
    lv_obj_set_style_text_font(dht_header, FONT_LABEL, 0);
    lv_obj_set_style_text_color(dht_header, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_letter_space(dht_header, 1, 0);
    lv_obj_align(dht_header, LV_ALIGN_TOP_LEFT, 0, 0);

    dht_value_label = lv_label_create(dht_card);
    lv_label_set_text(dht_value_label, "--.-C");
    lv_obj_set_style_text_font(dht_value_label, FONT_VALUE, 0);
    lv_obj_set_style_text_color(dht_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(dht_value_label, LV_ALIGN_TOP_LEFT, 0, 20);

    // Animated thermometer (temperature) on the left, animated
    // humidity gauge on the right — both real widgets built/updated
    // live, replacing the earlier static sparkline bars.
    dht_thermo = make_thermometer(dht_card, 2, 40, 14, 44, 18);

    dht_humidity_arc = lv_arc_create(dht_card);
    lv_obj_set_size(dht_humidity_arc, 40, 40);
    lv_obj_set_pos(dht_humidity_arc, 44, 42);
    lv_arc_set_rotation(dht_humidity_arc, 270);
    lv_arc_set_bg_angles(dht_humidity_arc, 0, 360);
    lv_arc_set_range(dht_humidity_arc, 0, 100);
    lv_obj_remove_style(dht_humidity_arc, NULL, LV_PART_KNOB | LV_STATE_ANY);
    lv_obj_clear_flag(dht_humidity_arc, LV_OBJ_FLAG_CLICKABLE); // display-only
    lv_obj_set_style_arc_color(dht_humidity_arc, lv_color_hex(0x0d1117), LV_PART_MAIN);
    lv_obj_set_style_arc_width(dht_humidity_arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(dht_humidity_arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(dht_humidity_arc, 5, LV_PART_INDICATOR);

    dht_humidity_value_label = lv_label_create(dht_humidity_arc);
    lv_label_set_text(dht_humidity_value_label, "--%");
    lv_obj_set_style_text_font(dht_humidity_value_label, FONT_SIDE_LABEL, 0);
    lv_obj_set_style_text_color(dht_humidity_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(dht_humidity_value_label);

    dht_sub_label = lv_label_create(dht_card);
    lv_label_set_text(dht_sub_label, "");
    lv_obj_set_style_text_font(dht_sub_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(dht_sub_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(dht_sub_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *ds_card = lv_obj_create(screen);
    ds_card_ref = ds_card;
    bare(ds_card);
    lv_obj_set_size(ds_card, CARD_W, CARD_H);
    lv_obj_set_pos(ds_card, CENTER_X + CARD_W + CARD_GAP, CARD_Y);
    lv_obj_set_style_bg_color(ds_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(ds_card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(ds_card, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_grad_dir(ds_card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(ds_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(ds_card, 1, 0);
    lv_obj_set_style_radius(ds_card, 2, 0);
    lv_obj_set_style_pad_all(ds_card, 10, 0);
    add_corner_brackets(screen, CENTER_X + CARD_W + CARD_GAP, CARD_Y, CARD_W, CARD_H, lv_palette_main(LV_PALETTE_CYAN));
    lv_obj_add_flag(ds_card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(ds_card, 6);
    add_press_feedback(ds_card, lv_color_hex(0x1c2430));
    lv_obj_add_event_cb(ds_card, on_ds_card_clicked, LV_EVENT_CLICKED, NULL);

    ds_dot = make_status_dot(ds_card);
    lv_obj_align(ds_dot, LV_ALIGN_TOP_RIGHT, 0, 2);
    start_pulse(ds_dot, 900);

    lv_obj_t *ds_header = lv_label_create(ds_card);
    lv_label_set_text(ds_header, "DS18B20");
    lv_obj_set_style_text_font(ds_header, FONT_LABEL, 0);
    lv_obj_set_style_text_color(ds_header, COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_letter_space(ds_header, 1, 0);
    lv_obj_align(ds_header, LV_ALIGN_TOP_LEFT, 0, 0);

    ds_value_label = lv_label_create(ds_card);
    lv_label_set_text(ds_value_label, "--.--C");
    lv_obj_set_style_text_font(ds_value_label, FONT_VALUE, 0);
    lv_obj_set_style_text_color(ds_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(ds_value_label, LV_ALIGN_TOP_LEFT, 0, 20);

    // Single-value sensor, no humidity to share the row with — bigger
    // thermometer, centered, than the DHT card's version.
    ds_thermo = make_thermometer(ds_card, (CARD_W - 20 - 16) / 2, 40, 16, 46, 20);

    ds_sub_label = lv_label_create(ds_card);
    lv_label_set_text(ds_sub_label, "");
    lv_obj_set_style_text_font(ds_sub_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(ds_sub_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(ds_sub_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // --- Status banner ---
    status_banner = lv_obj_create(screen);
    bare(status_banner);
    lv_obj_set_size(status_banner, CENTER_W, BANNER_H);
    lv_obj_set_pos(status_banner, CENTER_X, BANNER_Y);
    lv_obj_set_style_bg_color(status_banner, COLOR_BANNER_BG, 0);
    lv_obj_set_style_bg_opa(status_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(status_banner, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_grad_dir(status_banner, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_width(status_banner, 0, 0);
    lv_obj_set_style_radius(status_banner, 2, 0);
    lv_obj_set_style_pad_all(status_banner, 6, 0); // was 10 — reclaims content
                                                     // height the new status
                                                     // strip took from this
                                                     // banner (see theme.h)
    lv_obj_set_style_shadow_width(status_banner, 14, 0);
    lv_obj_set_style_shadow_spread(status_banner, 1, 0);
    lv_obj_set_style_shadow_color(status_banner, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_style_shadow_opa(status_banner, LV_OPA_20, 0);
    add_corner_brackets(screen, CENTER_X, BANNER_Y, CENTER_W, BANNER_H, lv_palette_main(LV_PALETTE_CYAN));
    lv_obj_add_flag(status_banner, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(status_banner, 8); // sits at the screen's bottom edge
    add_press_feedback(status_banner, lv_color_hex(0x1c2430));
    lv_obj_add_event_cb(status_banner, on_banner_clicked, LV_EVENT_CLICKED, NULL);

    // --- Static "STATUS" title — fixed at the top-center of the
    //     banner, deliberately NOT part of the scrolling ticker below
    //     it. Its own distinct pill background + wide letter-spacing
    //     gives it a badge/header look, separate from the live status
    //     value that scrolls underneath. ---
    lv_obj_t *status_title_box = lv_obj_create(status_banner);
    bare(status_title_box);
    lv_obj_set_size(status_title_box, 76, 18);
    lv_obj_align(status_title_box, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(status_title_box, lv_palette_darken(LV_PALETTE_CYAN, 3), 0);
    lv_obj_set_style_bg_opa(status_title_box, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(status_title_box, 9, 0);
    lv_obj_set_style_border_width(status_title_box, 1, 0);
    lv_obj_set_style_border_color(status_title_box, lv_palette_main(LV_PALETTE_CYAN), 0);

    lv_obj_t *status_title_label = lv_label_create(status_title_box);
    lv_label_set_text(status_title_label, "STATUS");
    lv_obj_set_style_text_font(status_title_label, FONT_SIDE_LABEL, 0);
    lv_obj_set_style_text_color(status_title_label, lv_palette_lighten(LV_PALETTE_CYAN, 2), 0);
    lv_obj_set_style_text_letter_space(status_title_label, 2, 0);
    lv_obj_center(status_title_label);

    // --- Vertical ticker: status value + reason line, scrolling
    //     upward together inside a clipped viewport, positioned below
    //     the static STATUS title above. Replaces the old horizontal
    //     marquee — a fixed-height banner otherwise can't show both a
    //     status line and a wrapped reason line at once without
    //     clipping one of them.
    lv_coord_t ticker_header_h = 22; // static STATUS title box + a small gap, reserved above the scrolling area
    lv_coord_t ticker_viewport_w = CENTER_W - 12; // banner width minus its own pad_all*2
    lv_coord_t ticker_viewport_h = (BANNER_H - 12) - ticker_header_h;
    lv_coord_t ticker_content_h = TICKER_CONTENT_H;

    lv_obj_t *ticker_viewport = lv_obj_create(status_banner);
    bare(ticker_viewport);
    lv_obj_set_size(ticker_viewport, ticker_viewport_w, ticker_viewport_h);
    lv_obj_align(ticker_viewport, LV_ALIGN_TOP_LEFT, 0, ticker_header_h);
    lv_obj_set_style_bg_opa(ticker_viewport, LV_OPA_TRANSP, 0);
    // Default LVGL clipping keeps ticker_content's off-screen portions
    // (above/below this viewport) hidden as it scrolls through.

    ticker_content = lv_obj_create(ticker_viewport);
    bare(ticker_content);
    lv_obj_set_size(ticker_content, ticker_viewport_w, ticker_content_h);
    lv_obj_set_pos(ticker_content, 0, ticker_viewport_h); // starts just below the viewport
    lv_obj_set_style_bg_opa(ticker_content, LV_OPA_TRANSP, 0);

    // Status value line: the "special font" treatment — bumped up to
    // FONT_TITLE (this project's largest available size) rather than
    // the sensor cards' FONT_VALUE, so it reads as the banner's clear
    // focal point. Shows just the value now (e.g. "OK") rather than
    // "Status: OK" — the word "Status" itself is the static title box
    // above, not repeated here.
    status_label = lv_label_create(ticker_content);
    lv_label_set_text(status_label, "Waiting for main-node...");
    lv_obj_set_style_text_font(status_label, FONT_TITLE, 0);
    lv_obj_set_style_text_color(status_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_set_style_text_letter_space(status_label, 1, 0);
    lv_obj_set_pos(status_label, 0, 0);

    // Reason line: dimmed cyan rather than plain grey, tying it into
    // the same accent color used by the corner brackets, accent line,
    // and comms glow elsewhere on this screen, instead of reading as
    // a generic secondary-text color.
    reason_label = lv_label_create(ticker_content);
    lv_label_set_text(reason_label, "");
    lv_obj_set_style_text_font(reason_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(reason_label, lv_palette_darken(LV_PALETTE_CYAN, 1), 0);
    lv_obj_set_width(reason_label, ticker_viewport_w);
    lv_label_set_long_mode(reason_label, LV_LABEL_LONG_WRAP); // wraps within the ticker instead of its own horizontal scroll
    lv_obj_set_pos(reason_label, 0, 26);

    // Loops: content starts just below the viewport (y = viewport_h),
    // scrolls up past fully-above (y = -content_h), then jumps back to
    // start and repeats. ~7s for a full pass — slow enough to read a
    // short status + reason without feeling rushed.
    start_vertical_ticker(ticker_content, ticker_viewport_h, -ticker_content_h, 7000);

    // Outer tactical frame — added LAST so it draws on top of the title
    // bar/banner rather than being covered by them at the screen edges.
    add_corner_brackets(screen, 0, 0, 320, 240, lv_palette_main(LV_PALETTE_CYAN));

    // Seed sparkline history with a flat baseline so the first real
    // reading doesn't look like a spike jumping from zero.
    for (int i = 0; i < SPARK_N; i++) {
        dht_temp_history[i] = 20.0f;
        ds_temp_history[i] = 20.0f;
    }
    history_initialized = true;

    // ============================================================
    // ============================================================
    // Layer 6 (touch/settings): Settings screen — cloud backend
    // toggle. A separate lv_screen (not a modal), reached via the
    // title bar's gear icon.
    // ============================================================
    settings_screen = lv_obj_create(NULL);
    bare(settings_screen);
    lv_obj_set_style_bg_color(settings_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(settings_screen, LV_OPA_COVER, 0);

    make_nav_header(settings_screen, "CLOUD BACKEND", on_settings_back_clicked);

    // Active-backend confirmation banner — same visual language as
    // the main dashboard's cards (dark fill, cyan accent) instead of
    // a plain label buried at the screen's edge.
    lv_obj_t *active_banner = lv_obj_create(settings_screen);
    bare(active_banner);
    lv_obj_set_size(active_banner, 304, 32);
    lv_obj_set_pos(active_banner, CARD_GAP, 40);
    lv_obj_set_style_bg_color(active_banner, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_bg_opa(active_banner, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(active_banner, 4, 0);
    lv_obj_set_style_border_side(active_banner, LV_BORDER_SIDE_LEFT, 0);
    lv_obj_set_style_border_width(active_banner, 3, 0);
    lv_obj_set_style_border_color(active_banner, lv_palette_main(LV_PALETTE_CYAN), 0);

    settings_active_label = lv_label_create(active_banner);
    lv_label_set_text(settings_active_label, "Active: —");
    lv_obj_set_style_text_font(settings_active_label, FONT_VALUE, 0);
    lv_obj_set_style_text_color(settings_active_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_align(settings_active_label, LV_ALIGN_LEFT_MID, 12, 0);

    const char *backend_names[3] = {"Google Sheets", "Firebase", "AWS"};
    lv_coord_t row_h = 36;
    lv_coord_t row_gap = 6;
    lv_coord_t row_y = 80;

    for (int i = 0; i < 3; i++) {
        lv_obj_t *row = lv_obj_create(settings_screen);
        bare(row);
        lv_obj_set_size(row, 304, row_h);
        lv_obj_set_pos(row, CARD_GAP, row_y + i * (row_h + row_gap));
        lv_obj_set_style_bg_color(row, lv_color_hex(0x161b22), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_color(row, COLOR_CARD_BORDER, 0);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        add_press_feedback(row, lv_color_hex(0x232a35));
        lv_obj_add_event_cb(row, on_backend_option_clicked, LV_EVENT_CLICKED, (void *)(intptr_t)i);

        settings_option_dot[i] = make_status_dot(row);
        lv_obj_align(settings_option_dot[i], LV_ALIGN_LEFT_MID, 12, 0);

        lv_obj_t *row_label = lv_label_create(row);
        lv_label_set_text(row_label, backend_names[i]);
        lv_obj_set_style_text_font(row_label, FONT_VALUE, 0);
        lv_obj_set_style_text_color(row_label, COLOR_TEXT_PRIMARY, 0);
        lv_obj_align_to(row_label, settings_option_dot[i], LV_ALIGN_OUT_RIGHT_MID, 12, 0);

        // Checkmark is the clearer "confirmed active" signal — shown
        // or hidden in ui_update(), not just a recolored dot.
        settings_option_check[i] = lv_label_create(row);
        lv_label_set_text(settings_option_check[i], LV_SYMBOL_OK);
        lv_obj_set_style_text_font(settings_option_check[i], FONT_LABEL, 0);
        lv_obj_set_style_text_color(settings_option_check[i], lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_obj_align(settings_option_check[i], LV_ALIGN_RIGHT_MID, -12, 0);
        lv_obj_add_flag(settings_option_check[i], LV_OBJ_FLAG_HIDDEN);
    }

    // ============================================================
    // Layer 6 (touch/settings): Detail screen — larger view of
    // whichever sensor's card was tapped. Reuses the same retained
    // history the dashboard's small sparklines already track, now
    // rendered with LVGL's own arc and chart widgets instead of
    // hand-rolled bars.
    // ============================================================
    detail_screen = lv_obj_create(NULL);
    bare(detail_screen);
    lv_obj_set_style_bg_color(detail_screen, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(detail_screen, LV_OPA_COVER, 0);

    make_nav_header(detail_screen, "SENSOR DETAIL", on_detail_back_clicked, &detail_title_label);

    // Real LVGL arc gauge — read-only (no knob, not draggable), just
    // used as a radial value display instead of a plain number.
    detail_arc = lv_arc_create(detail_screen);
    lv_obj_set_size(detail_arc, 130, 130);
    lv_obj_align(detail_arc, LV_ALIGN_TOP_MID, 0, 42);
    lv_arc_set_rotation(detail_arc, 135);
    lv_arc_set_bg_angles(detail_arc, 0, 270);
    lv_arc_set_range(detail_arc, 0, 50); // 0-50C covers both sensors' realistic range
    lv_obj_remove_style(detail_arc, NULL, LV_PART_KNOB | LV_STATE_ANY); // no draggable handle
    lv_obj_clear_flag(detail_arc, LV_OBJ_FLAG_CLICKABLE); // display-only, not user-adjustable
    lv_obj_set_style_arc_color(detail_arc, lv_color_hex(0x232a35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(detail_arc, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(detail_arc, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(detail_arc, 12, LV_PART_INDICATOR);

    detail_value_label = lv_label_create(detail_arc);
    lv_label_set_text(detail_value_label, "--.-C");
    lv_obj_set_style_text_font(detail_value_label, FONT_TITLE, 0);
    lv_obj_set_style_text_color(detail_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(detail_value_label);

    // Real LVGL chart for history — native gridlines and line-drawing
    // instead of a manual bar approximation.
    detail_chart = lv_chart_create(detail_screen);
    lv_obj_set_size(detail_chart, 304, 55);
    lv_obj_align(detail_chart, LV_ALIGN_BOTTOM_MID, 0, -8);
    lv_obj_set_style_bg_color(detail_chart, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_bg_opa(detail_chart, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(detail_chart, 1, 0);
    lv_obj_set_style_border_color(detail_chart, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_radius(detail_chart, 4, 0);
    lv_chart_set_type(detail_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(detail_chart, SPARK_N);
    lv_chart_set_range(detail_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 50);
    lv_chart_set_div_line_count(detail_chart, 3, 0);
    detail_chart_series = lv_chart_add_series(detail_chart, lv_palette_main(LV_PALETTE_CYAN), LV_CHART_AXIS_PRIMARY_Y);

    // --- DHT22 view: temp + humidity arcs side by side. Smaller than
    //     the DS18B20 view's single arc (110px vs 130px) since two
    //     need to fit side by side, and no chart here — there's no
    //     retained humidity history to chart alongside temperature. ---
    detail_dht_temp_arc = lv_arc_create(detail_screen);
    lv_obj_set_size(detail_dht_temp_arc, 110, 110);
    lv_obj_set_pos(detail_dht_temp_arc, 40, 50);
    lv_arc_set_rotation(detail_dht_temp_arc, 135);
    lv_arc_set_bg_angles(detail_dht_temp_arc, 0, 270);
    lv_arc_set_range(detail_dht_temp_arc, 0, 50);
    lv_obj_remove_style(detail_dht_temp_arc, NULL, LV_PART_KNOB | LV_STATE_ANY);
    lv_obj_clear_flag(detail_dht_temp_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(detail_dht_temp_arc, lv_color_hex(0x232a35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(detail_dht_temp_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(detail_dht_temp_arc, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(detail_dht_temp_arc, 10, LV_PART_INDICATOR);

    detail_dht_temp_value_label = lv_label_create(detail_dht_temp_arc);
    lv_label_set_text(detail_dht_temp_value_label, "--.-C");
    lv_obj_set_style_text_font(detail_dht_temp_value_label, FONT_VALUE, 0);
    lv_obj_set_style_text_color(detail_dht_temp_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(detail_dht_temp_value_label);

    detail_dht_temp_caption = lv_label_create(detail_screen);
    lv_label_set_text(detail_dht_temp_caption, "Temperature");
    lv_obj_set_style_text_font(detail_dht_temp_caption, FONT_SIDE_LABEL, 0);
    lv_obj_set_style_text_color(detail_dht_temp_caption, COLOR_TEXT_MUTED, 0);
    lv_obj_align(detail_dht_temp_caption, LV_ALIGN_TOP_LEFT, 40, 164);

    detail_dht_humidity_arc = lv_arc_create(detail_screen);
    lv_obj_set_size(detail_dht_humidity_arc, 110, 110);
    lv_obj_set_pos(detail_dht_humidity_arc, 170, 50);
    lv_arc_set_rotation(detail_dht_humidity_arc, 135);
    lv_arc_set_bg_angles(detail_dht_humidity_arc, 0, 270);
    lv_arc_set_range(detail_dht_humidity_arc, 0, 100);
    lv_obj_remove_style(detail_dht_humidity_arc, NULL, LV_PART_KNOB | LV_STATE_ANY);
    lv_obj_clear_flag(detail_dht_humidity_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(detail_dht_humidity_arc, lv_color_hex(0x232a35), LV_PART_MAIN);
    lv_obj_set_style_arc_width(detail_dht_humidity_arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(detail_dht_humidity_arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(detail_dht_humidity_arc, 10, LV_PART_INDICATOR);

    detail_dht_humidity_value_label = lv_label_create(detail_dht_humidity_arc);
    lv_label_set_text(detail_dht_humidity_value_label, "--%");
    lv_obj_set_style_text_font(detail_dht_humidity_value_label, FONT_VALUE, 0);
    lv_obj_set_style_text_color(detail_dht_humidity_value_label, COLOR_TEXT_PRIMARY, 0);
    lv_obj_center(detail_dht_humidity_value_label);

    detail_dht_humidity_caption = lv_label_create(detail_screen);
    lv_label_set_text(detail_dht_humidity_caption, "Humidity");
    lv_obj_set_style_text_font(detail_dht_humidity_caption, FONT_SIDE_LABEL, 0);
    lv_obj_set_style_text_color(detail_dht_humidity_caption, COLOR_TEXT_MUTED, 0);
    lv_obj_align(detail_dht_humidity_caption, LV_ALIGN_TOP_LEFT, 170, 164);

    // Default state matches detail_which's default (0 = DHT22) — the
    // DS18B20 view starts hidden until its card is actually tapped.
    lv_obj_add_flag(detail_arc, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(detail_chart, LV_OBJ_FLAG_HIDDEN);
}

void ui_update(const CommsPacket &pkt, bool haveEverReceivedPacket, bool linkStale) {
    if (!haveEverReceivedPacket) {
        return; // widgets already show their "waiting..." placeholders
    }

    char buf[64];

    if (pkt.dht_read_ok) {
        snprintf(buf, sizeof(buf), "%.1fC", pkt.dht_temperature_c);
    } else {
        // %.1f of a NaN prints the literal text "nanC" — this is the
        // same class of bug the Google Sheets JSON had (Layer 3's
        // formatNumberOrNull fix). Guard it here the same way: only
        // format the number when the read actually succeeded.
        snprintf(buf, sizeof(buf), "--.-C");
    }
    lv_label_set_text(dht_value_label, buf);
    set_dot_color(dht_dot,
        pkt.dht_read_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

    if (pkt.dht_read_ok) {
        push_history(dht_temp_history, pkt.dht_temperature_c);
    }
    set_thermometer_value(dht_thermo, pkt.dht_temperature_c, pkt.dht_read_ok, 0.0f, 50.0f, 44);

    if (pkt.dht_read_ok) {
        snprintf(buf, sizeof(buf), "%.0f%%", pkt.dht_humidity_pct);
    } else {
        // Same NaN-formatting guard as the temperature values —
        // dht_humidity_pct is also NaN whenever dht_read_ok is false.
        snprintf(buf, sizeof(buf), "--%%");
    }
    lv_label_set_text(dht_humidity_value_label, buf);
    int32_t humidityTarget = pkt.dht_read_ok ? (int32_t)pkt.dht_humidity_pct : 0;
    animate_arc_to(dht_humidity_arc, dht_humidity_last_val, humidityTarget, 400);
    dht_humidity_last_val = humidityTarget;
    lv_obj_set_style_arc_color(dht_humidity_arc,
        pkt.dht_read_ok ? lv_palette_main(LV_PALETTE_BLUE) : lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);

    // Now just a failure indicator, matching ds_sub_label's pattern —
    // the humidity value itself lives in the arc gauge above, so
    // showing it again here would just duplicate it.
    lv_label_set_text(dht_sub_label, pkt.dht_read_ok ? "" : "READ FAILING");
    lv_obj_set_style_text_color(dht_sub_label,
        pkt.dht_read_ok ? COLOR_TEXT_MUTED : lv_palette_main(LV_PALETTE_RED), 0);

    if (pkt.ds18b20_read_ok) {
        snprintf(buf, sizeof(buf), "%.2fC", pkt.ds18b20_temperature_c);
    } else {
        snprintf(buf, sizeof(buf), "--.-C");
    }
    lv_label_set_text(ds_value_label, buf);
    set_dot_color(ds_dot,
        pkt.ds18b20_read_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

    if (pkt.ds18b20_read_ok) {
        push_history(ds_temp_history, pkt.ds18b20_temperature_c);
    }
    set_thermometer_value(ds_thermo, pkt.ds18b20_temperature_c, pkt.ds18b20_read_ok, 0.0f, 50.0f, 46);

    lv_label_set_text(ds_sub_label, pkt.ds18b20_read_ok ? "" : "READ FAILING");
    lv_obj_set_style_text_color(ds_sub_label,
        pkt.ds18b20_read_ok ? COLOR_TEXT_MUTED : lv_palette_main(LV_PALETTE_RED), 0);

    // Layer 6: system-status strip. If the link itself is stale, these
    // are whatever main-node last reported before it went quiet — worth
    // showing dimmed rather than a flat grey "unknown", since a stale
    // WiFi=green from 30 seconds ago is still more informative than
    // nothing while you're figuring out why the link dropped.
    set_icon_status_color(wifi_dot,
        pkt.wifi_connected ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));
    set_icon_status_color(sd_dot,
        pkt.sd_card_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));
    set_icon_status_color(cloud_dot,
        pkt.cloud_sync_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

    uint8_t effectiveStatus = linkStale ? 3 : pkt.health_status;
    lv_obj_set_style_bg_color(status_banner, statusToBgColor(effectiveStatus), 0);

    snprintf(buf, sizeof(buf), "%s", linkStale ? "LINK LOST" : statusToStr(pkt.health_status));
    lv_label_set_text(status_label, buf);
    lv_obj_set_style_text_color(status_label, statusToColor(effectiveStatus), 0);

    lv_label_set_text(reason_label, linkStale ? "No packet received in over 10 seconds" : pkt.health_reason);

    // Layer 6 (touch/settings): keep the Settings and Detail screens
    // fresh even while the main dashboard is the one actually visible —
    // switching screens should never show stale data from whenever
    // that screen was last on top.
    snprintf(buf, sizeof(buf), "Active: %s", cloudBackendName(pkt.active_cloud_backend));
    lv_label_set_text(settings_active_label, buf);
    for (int i = 0; i < 3; i++) {
        bool isActive = (pkt.active_cloud_backend == i);
        set_dot_color(settings_option_dot[i],
            isActive ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_GREY));
        if (isActive) {
            lv_obj_clear_flag(settings_option_check[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(settings_option_check[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

    // Both view sets update every cycle regardless of which one is
    // currently visible (visibility is toggled only on card tap, in
    // on_dht_card_clicked()/on_ds_card_clicked()) — so whichever one
    // you switch to next is already showing fresh data, not whatever
    // was last on screen.

    // --- DS18B20 view: single arc + chart ---
    if (pkt.ds18b20_read_ok) {
        snprintf(buf, sizeof(buf), "%.2fC", pkt.ds18b20_temperature_c);
    } else {
        // Same NaN-formatting guard as the main dashboard's cards —
        // a failed read's value is NaN, and %.2f of NaN prints the
        // literal text "nanC", not a clean placeholder.
        snprintf(buf, sizeof(buf), "--.-C");
    }
    lv_label_set_text(detail_value_label, buf);

    lv_color_t dsColor = pkt.ds18b20_read_ok ? lv_palette_main(LV_PALETTE_CYAN) : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_arc_color(detail_arc, dsColor, LV_PART_INDICATOR);
    // Casting a NaN float to int32_t is undefined/implementation-defined
    // behavior in C++ — hold the arc at its last good position instead
    // of feeding it a NaN-derived value when the read has failed.
    if (pkt.ds18b20_read_ok) {
        lv_arc_set_value(detail_arc, (int32_t)pkt.ds18b20_temperature_c);
    }

    for (int i = 0; i < SPARK_N; i++) {
        lv_chart_set_value_by_id(detail_chart, detail_chart_series, i, (int32_t)ds_temp_history[i]);
    }
    // Series color lives on the series struct itself in LVGL — more
    // reliable than guessing at a style-part selector for something
    // I can't render and check.
    detail_chart_series->color = dsColor;
    lv_chart_refresh(detail_chart);

    // --- DHT22 view: temp + humidity arcs side by side ---
    if (pkt.dht_read_ok) {
        snprintf(buf, sizeof(buf), "%.1fC", pkt.dht_temperature_c);
    } else {
        snprintf(buf, sizeof(buf), "--.-C");
    }
    lv_label_set_text(detail_dht_temp_value_label, buf);

    lv_color_t dhtTempColor = pkt.dht_read_ok ? lv_palette_main(LV_PALETTE_CYAN) : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_arc_color(detail_dht_temp_arc, dhtTempColor, LV_PART_INDICATOR);
    if (pkt.dht_read_ok) {
        lv_arc_set_value(detail_dht_temp_arc, (int32_t)pkt.dht_temperature_c);
    }

    if (pkt.dht_read_ok) {
        snprintf(buf, sizeof(buf), "%.0f%%", pkt.dht_humidity_pct);
    } else {
        // dht_humidity_pct is also NaN whenever dht_read_ok is false —
        // same guard, same reason.
        snprintf(buf, sizeof(buf), "--%%");
    }
    lv_label_set_text(detail_dht_humidity_value_label, buf);

    lv_color_t dhtHumidityColor = pkt.dht_read_ok ? lv_palette_main(LV_PALETTE_BLUE) : lv_palette_main(LV_PALETTE_RED);
    lv_obj_set_style_arc_color(detail_dht_humidity_arc, dhtHumidityColor, LV_PART_INDICATOR);
    if (pkt.dht_read_ok) {
        lv_arc_set_value(detail_dht_humidity_arc, (int32_t)pkt.dht_humidity_pct);
    }

    // Live-update heartbeat — a brief border flash on each refresh
    // cycle, so the dashboard visibly "breathes" with new data rather
    // than looking static between changes.
    flash_border(dht_card_ref);
    flash_border(ds_card_ref);
}

void ui_set_comms_status(const char *transportName, bool active) {
    // transportName is intentionally unused now — the side panel is
    // icon-only (see make_status_icon_badge() in status_icons.cpp),
    // no visible text label to update. Parameter kept in ui.h's public
    // signature so main.cpp doesn't need to change, and in case a
    // future layer wants the text back.
    (void)transportName;
    set_icon_status_color(comms_dot,
        active ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));
}