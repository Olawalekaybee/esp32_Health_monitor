#include "ui.h"
#include "theme.h"
#include <lvgl.h>
#include <cstdio>
#include <cstring>

// --- Widgets, created once in ui_create() ---
static lv_obj_t *dht_value_label;
static lv_obj_t *dht_sub_label;
static lv_obj_t *dht_dot;
static lv_obj_t *dht_spark_bars[SPARK_N];
static lv_obj_t *ds_value_label;
static lv_obj_t *ds_sub_label;
static lv_obj_t *ds_dot;
static lv_obj_t *ds_spark_bars[SPARK_N];
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
static void bare(lv_obj_t *obj) {
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// Small colored dot used as a per-sensor status indicator, independent
// of the numeric value color — lets you tell "reading is stale/failing"
// apart from "reading is just cold" at a glance.
static lv_obj_t *make_status_dot(lv_obj_t *parent) {
    lv_obj_t *dot = lv_obj_create(parent);
    bare(dot);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    // Soft glow behind the dot, color-matched whenever set_dot_color() runs.
    lv_obj_set_style_shadow_width(dot, 10, 0);
    lv_obj_set_style_shadow_spread(dot, 2, 0);
    lv_obj_set_style_shadow_color(dot, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_set_style_shadow_opa(dot, LV_OPA_50, 0);
    return dot;
}

// Sets a dot's fill AND its glow color together, so the glow always
// matches whatever status color the dot is showing.
static void set_dot_color(lv_obj_t *dot, lv_color_t color) {
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_shadow_color(dot, color, 0);
}

// Layer 6: compact pill badge for the system-status strip — same
// visual language as the existing title-bar comms badge (dark fill,
// thin border, rounded ends, status dot + fixed label), factored out
// here since we now need three of them side by side instead of one.
// The label text never changes (e.g. always "WiFi") — only the dot's
// color reflects live status, matching how the comms badge already
// behaves.
static lv_obj_t *make_mini_badge(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                   lv_coord_t w, lv_coord_t h, const char *label_text) {
    lv_obj_t *badge = lv_obj_create(parent);
    bare(badge);
    lv_obj_set_size(badge, w, h);
    lv_obj_set_pos(badge, x, y);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, h / 2, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_border_color(badge, COLOR_CARD_BORDER, 0);

    lv_obj_t *dot = make_status_dot(badge);
    lv_obj_align(dot, LV_ALIGN_LEFT_MID, 6, 0);

    lv_obj_t *label = lv_label_create(badge);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(label, COLOR_TEXT_MUTED, 0);
    lv_obj_align_to(label, dot, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    return dot; // caller only ever needs to recolor the dot afterward
}

// Small angular "target-lock" bracket — the tactical HUD marks visible
// in sci-fi interface reference art. Cheap to render (two thin flat
// rectangles, no radius/shadow). Always attached to `parent` using
// ABSOLUTE coordinates (not relative to any padded content area) —
// pass the screen as parent and the panel's own on-screen x/y/w/h, so
// there's no ambiguity about padding offsetting the bracket inward.
static void add_corner_bracket(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                bool right, bool bottom, lv_color_t color) {
    const int len = 10;
    const int thick = 2;
    lv_coord_t hx = right ? x - len : x;
    lv_coord_t hy = bottom ? y - thick : y;
    lv_coord_t vx = right ? x - thick : x;
    lv_coord_t vy = bottom ? y - len : y;

    lv_obj_t *h = lv_obj_create(parent);
    bare(h);
    lv_obj_set_size(h, len, thick);
    lv_obj_set_pos(h, hx, hy);
    lv_obj_set_style_bg_color(h, color, 0);
    lv_obj_set_style_bg_opa(h, LV_OPA_COVER, 0);

    lv_obj_t *v = lv_obj_create(parent);
    bare(v);
    lv_obj_set_size(v, thick, len);
    lv_obj_set_pos(v, vx, vy);
    lv_obj_set_style_bg_color(v, color, 0);
    lv_obj_set_style_bg_opa(v, LV_OPA_COVER, 0);
}

// All 4 corners of a w x h area whose top-left sits at (ox, oy) in
// `parent`'s absolute coordinate space.
static void add_corner_brackets(lv_obj_t *parent, lv_coord_t ox, lv_coord_t oy,
                                 lv_coord_t w, lv_coord_t h, lv_color_t color) {
    add_corner_bracket(parent, ox,     oy,     false, false, color);
    add_corner_bracket(parent, ox + w, oy,     true,  false, color);
    add_corner_bracket(parent, ox,     oy + h, false, true,  color);
    add_corner_bracket(parent, ox + w, oy + h, true,  true,  color);
}

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

// --- Animations: a soft "breathing" pulse, a glowing accent line, and
//     a one-shot border "flash" pulse on each data refresh — for a
//     more alive, futuristic feel. Kept intentionally modest — this
//     hardware has shown SPI/render sensitivity earlier in this
//     project, so heavier effects (full-screen sweeps, particles)
//     aren't worth the risk of stutter for a cosmetic upgrade. ---

static void opa_pulse_anim_cb(void *var, int32_t value) {
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)value, 0);
}

static void start_pulse(lv_obj_t *obj, uint32_t half_cycle_ms) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, opa_pulse_anim_cb);
    lv_anim_set_values(&a, LV_OPA_40, LV_OPA_COVER);
    lv_anim_set_time(&a, half_cycle_ms);
    lv_anim_set_playback_time(&a, half_cycle_ms);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void accent_glow_anim_cb(void *var, int32_t value) {
    lv_color_t dim = lv_palette_darken(LV_PALETTE_CYAN, 2);
    lv_color_t bright = lv_palette_main(LV_PALETTE_CYAN);
    lv_obj_set_style_bg_color((lv_obj_t *)var, lv_color_mix(bright, dim, (uint8_t)value), 0);
}

static void start_accent_glow(lv_obj_t *obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, accent_glow_anim_cb);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_playback_time(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void border_flash_anim_cb(void *var, int32_t value) {
    lv_color_t bright = lv_palette_lighten(LV_PALETTE_CYAN, 2);
    lv_obj_set_style_border_color((lv_obj_t *)var, lv_color_mix(bright, COLOR_CARD_BORDER, (uint8_t)value), 0);
}

// One-shot "this card just got fresh data" flash — bright border
// fading back to normal, not a repeating animation.
static void flash_border(lv_obj_t *obj) {
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, border_flash_anim_cb);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_time(&a, 600);
    lv_anim_set_repeat_count(&a, 1);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

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

void ui_create(void) {
    lv_obj_t *screen = lv_screen_active();
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

    // --- Comms status: pill-shaped badge (dark fill, thin border,
    //     rounded ends), matching "CONNECTED"-style status badges. ---
    lv_obj_t *comms_badge = lv_obj_create(title_bar);
    bare(comms_badge);
    lv_obj_set_size(comms_badge, 84, 18);
    lv_obj_align(comms_badge, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_style_bg_color(comms_badge, lv_color_hex(0x161b22), 0);
    lv_obj_set_style_bg_opa(comms_badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(comms_badge, 9, 0);
    lv_obj_set_style_border_width(comms_badge, 1, 0);
    lv_obj_set_style_border_color(comms_badge, COLOR_CARD_BORDER, 0);

    comms_dot = make_status_dot(comms_badge);
    lv_obj_align(comms_dot, LV_ALIGN_LEFT_MID, 6, 0);
    start_pulse(comms_dot, 900);

    comms_label = lv_label_create(comms_badge);
    lv_label_set_text(comms_label, "");
    lv_obj_set_style_text_font(comms_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(comms_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align_to(comms_label, comms_dot, LV_ALIGN_OUT_RIGHT_MID, 6, 0);

    lv_obj_t *accent_line = lv_obj_create(screen);
    bare(accent_line);
    lv_obj_set_size(accent_line, 320, ACCENT_LINE_H);
    lv_obj_set_pos(accent_line, 0, TITLE_BAR_H);
    lv_obj_set_style_bg_opa(accent_line, LV_OPA_COVER, 0);
    start_accent_glow(accent_line);

    // --- System-status strip: WiFi / SD / Cloud, Layer 6's main
    //     addition — surfaces state that previously only existed in
    //     main-node's own Serial log. ---
    wifi_dot = make_mini_badge(screen, CARD_GAP, STATUS_STRIP_Y,
                                STATUS_BADGE_W, STATUS_STRIP_H, "WiFi");
    start_pulse(wifi_dot, 900);

    sd_dot = make_mini_badge(screen, CARD_GAP + STATUS_BADGE_W + STATUS_BADGE_GAP, STATUS_STRIP_Y,
                               STATUS_BADGE_W, STATUS_STRIP_H, "SD");
    start_pulse(sd_dot, 900);

    cloud_dot = make_mini_badge(screen, CARD_GAP + 2 * (STATUS_BADGE_W + STATUS_BADGE_GAP), STATUS_STRIP_Y,
                                  STATUS_BADGE_W, STATUS_STRIP_H, "Cloud");
    start_pulse(cloud_dot, 900);

    // --- Sensor cards ---
    lv_obj_t *dht_card = lv_obj_create(screen);
    dht_card_ref = dht_card;
    bare(dht_card);
    lv_obj_set_size(dht_card, CARD_W, CARD_H);
    lv_obj_set_pos(dht_card, CARD_GAP, CARD_Y);
    lv_obj_set_style_bg_color(dht_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(dht_card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(dht_card, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_grad_dir(dht_card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(dht_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(dht_card, 1, 0);
    lv_obj_set_style_radius(dht_card, 2, 0);
    lv_obj_set_style_pad_all(dht_card, 10, 0);
    add_corner_brackets(screen, CARD_GAP, CARD_Y, CARD_W, CARD_H, lv_palette_main(LV_PALETTE_CYAN));

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

    create_sparkline(dht_card, 0, 52, dht_spark_bars);

    dht_sub_label = lv_label_create(dht_card);
    lv_label_set_text(dht_sub_label, "humidity --%");
    lv_obj_set_style_text_font(dht_sub_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(dht_sub_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(dht_sub_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    lv_obj_t *ds_card = lv_obj_create(screen);
    ds_card_ref = ds_card;
    bare(ds_card);
    lv_obj_set_size(ds_card, CARD_W, CARD_H);
    lv_obj_set_pos(ds_card, CARD_GAP + CARD_W + CARD_GAP, CARD_Y);
    lv_obj_set_style_bg_color(ds_card, COLOR_CARD_BG, 0);
    lv_obj_set_style_bg_opa(ds_card, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_grad_color(ds_card, lv_color_hex(0x0d1117), 0);
    lv_obj_set_style_bg_grad_dir(ds_card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_border_color(ds_card, COLOR_CARD_BORDER, 0);
    lv_obj_set_style_border_width(ds_card, 1, 0);
    lv_obj_set_style_radius(ds_card, 2, 0);
    lv_obj_set_style_pad_all(ds_card, 10, 0);
    add_corner_brackets(screen, CARD_GAP + CARD_W + CARD_GAP, CARD_Y, CARD_W, CARD_H, lv_palette_main(LV_PALETTE_CYAN));

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

    create_sparkline(ds_card, 0, 52, ds_spark_bars);

    ds_sub_label = lv_label_create(ds_card);
    lv_label_set_text(ds_sub_label, "");
    lv_obj_set_style_text_font(ds_sub_label, FONT_LABEL, 0);
    lv_obj_set_style_text_color(ds_sub_label, COLOR_TEXT_MUTED, 0);
    lv_obj_align(ds_sub_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    // --- Status banner ---
    status_banner = lv_obj_create(screen);
    bare(status_banner);
    lv_obj_set_size(status_banner, 304, BANNER_H);
    lv_obj_set_pos(status_banner, CARD_GAP, BANNER_Y);
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
    add_corner_brackets(screen, CARD_GAP, BANNER_Y, 304, BANNER_H, lv_palette_main(LV_PALETTE_CYAN));

    // --- Vertical ticker: status line + reason line, scrolling
    //     upward together inside a clipped viewport. Replaces the old
    //     horizontal marquee — a fixed-height banner otherwise can't
    //     show both a status line and a wrapped reason line at once
    //     without clipping one of them.
    lv_coord_t ticker_viewport_w = 304 - 12; // banner width minus its own pad_all*2
    lv_coord_t ticker_viewport_h = BANNER_H - 12;
    lv_coord_t ticker_content_h = 60; // status line + up to 2 wrapped reason lines

    lv_obj_t *ticker_viewport = lv_obj_create(status_banner);
    bare(ticker_viewport);
    lv_obj_set_size(ticker_viewport, ticker_viewport_w, ticker_viewport_h);
    lv_obj_align(ticker_viewport, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(ticker_viewport, LV_OPA_TRANSP, 0);
    // Default LVGL clipping keeps ticker_content's off-screen portions
    // (above/below this viewport) hidden as it scrolls through.

    ticker_content = lv_obj_create(ticker_viewport);
    bare(ticker_content);
    lv_obj_set_size(ticker_content, ticker_viewport_w, ticker_content_h);
    lv_obj_set_pos(ticker_content, 0, ticker_viewport_h); // starts just below the viewport
    lv_obj_set_style_bg_opa(ticker_content, LV_OPA_TRANSP, 0);

    // Status line: the "special font" treatment — bumped up to
    // FONT_TITLE (this project's largest available size) rather than
    // the sensor cards' FONT_VALUE, so the status line reads as the
    // banner's clear focal point rather than matching card body text.
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
}

void ui_update(const CommsPacket &pkt, bool haveEverReceivedPacket, bool linkStale) {
    if (!haveEverReceivedPacket) {
        return; // widgets already show their "waiting..." placeholders
    }

    char buf[64];

    snprintf(buf, sizeof(buf), "%.1fC", pkt.dht_temperature_c);
    lv_label_set_text(dht_value_label, buf);
    set_dot_color(dht_dot,
        pkt.dht_read_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

    if (pkt.dht_read_ok) {
        push_history(dht_temp_history, pkt.dht_temperature_c);
    }
    update_sparkline(dht_spark_bars, dht_temp_history, 0.0f, 50.0f,
                      pkt.dht_read_ok ? lv_palette_main(LV_PALETTE_CYAN) : lv_palette_main(LV_PALETTE_RED));

    snprintf(buf, sizeof(buf), pkt.dht_read_ok ? "humidity %.0f%%" : "READ FAILING", pkt.dht_humidity_pct);
    lv_label_set_text(dht_sub_label, buf);
    lv_obj_set_style_text_color(dht_sub_label,
        pkt.dht_read_ok ? COLOR_TEXT_MUTED : lv_palette_main(LV_PALETTE_RED), 0);

    snprintf(buf, sizeof(buf), "%.2fC", pkt.ds18b20_temperature_c);
    lv_label_set_text(ds_value_label, buf);
    set_dot_color(ds_dot,
        pkt.ds18b20_read_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

    if (pkt.ds18b20_read_ok) {
        push_history(ds_temp_history, pkt.ds18b20_temperature_c);
    }
    update_sparkline(ds_spark_bars, ds_temp_history, 0.0f, 50.0f,
                      pkt.ds18b20_read_ok ? lv_palette_main(LV_PALETTE_CYAN) : lv_palette_main(LV_PALETTE_RED));

    lv_label_set_text(ds_sub_label, pkt.ds18b20_read_ok ? "" : "READ FAILING");
    lv_obj_set_style_text_color(ds_sub_label,
        pkt.ds18b20_read_ok ? COLOR_TEXT_MUTED : lv_palette_main(LV_PALETTE_RED), 0);

    // Layer 6: system-status strip. If the link itself is stale, these
    // are whatever main-node last reported before it went quiet — worth
    // showing dimmed rather than a flat grey "unknown", since a stale
    // WiFi=green from 30 seconds ago is still more informative than
    // nothing while you're figuring out why the link dropped.
    set_dot_color(wifi_dot,
        pkt.wifi_connected ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));
    set_dot_color(sd_dot,
        pkt.sd_card_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));
    set_dot_color(cloud_dot,
        pkt.cloud_sync_ok ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));

    uint8_t effectiveStatus = linkStale ? 3 : pkt.health_status;
    lv_obj_set_style_bg_color(status_banner, statusToBgColor(effectiveStatus), 0);

    snprintf(buf, sizeof(buf), "Status: %s", linkStale ? "LINK LOST" : statusToStr(pkt.health_status));
    lv_label_set_text(status_label, buf);
    lv_obj_set_style_text_color(status_label, statusToColor(effectiveStatus), 0);

    lv_label_set_text(reason_label, linkStale ? "No packet received in over 10 seconds" : pkt.health_reason);

    // Live-update heartbeat — a brief border flash on each refresh
    // cycle, so the dashboard visibly "breathes" with new data rather
    // than looking static between changes.
    flash_border(dht_card_ref);
    flash_border(ds_card_ref);
}

void ui_set_comms_status(const char *transportName, bool active) {
    lv_label_set_text(comms_label, transportName);
    set_dot_color(comms_dot,
        active ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_RED));
}