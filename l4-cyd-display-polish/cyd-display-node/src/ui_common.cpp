#include "ui_common.h"

void bare(lv_obj_t *obj) {
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *make_status_dot(lv_obj_t *parent) {
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

void set_dot_color(lv_obj_t *dot, lv_color_t color) {
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_shadow_color(dot, color, 0);
}

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

void add_corner_brackets(lv_obj_t *parent, lv_coord_t ox, lv_coord_t oy,
                          lv_coord_t w, lv_coord_t h, lv_color_t color) {
    add_corner_bracket(parent, ox,     oy,     false, false, color);
    add_corner_bracket(parent, ox + w, oy,     true,  false, color);
    add_corner_bracket(parent, ox,     oy + h, false, true,  color);
    add_corner_bracket(parent, ox + w, oy + h, true,  true,  color);
}

void add_press_feedback(lv_obj_t *obj, lv_color_t pressed_color) {
    lv_obj_set_style_bg_color(obj, pressed_color, LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_PRESSED);
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

void start_pulse(lv_obj_t *obj, uint32_t half_cycle_ms) {
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

void start_accent_glow(lv_obj_t *obj) {
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

void flash_border(lv_obj_t *obj) {
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