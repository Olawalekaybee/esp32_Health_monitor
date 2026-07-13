#pragma once
#include <lvgl.h>
#include "theme.h"
// ============================================================
// ui_common.h — small, generic LVGL widget helpers shared across
// multiple .cpp files (ui.cpp, status_icons.cpp, and future screen
// files). These have no dependency on any specific screen's layout —
// that's what makes them safe to share rather than duplicate.
//
// This is the first real step of the promised modular split: pulling
// out genuinely-shared code that multiple files now need, rather than
// duplicating it. Splitting the existing screens (main/settings/detail)
// into their own files is the next step, once this is confirmed
// working — deliberately not attempted in the same pass as new
// layout/feature changes.
// ============================================================

// Strips all default LVGL styling from a fresh object and turns off
// scrolling — every custom-styled widget in this UI starts from this
// blank slate rather than fighting the default theme (which is
// disabled globally anyway, but this keeps every widget's appearance
// fully explicit and predictable regardless).
void bare(lv_obj_t *obj);

// Small colored dot used as a per-sensor status indicator, independent
// of the numeric value color — lets you tell "reading is stale/failing"
// apart from "reading is just cold" at a glance.
lv_obj_t *make_status_dot(lv_obj_t *parent);

// Sets a dot's fill AND its glow color together, so the glow always
// matches whatever status color the dot is showing.
void set_dot_color(lv_obj_t *dot, lv_color_t color);

// Sci-fi "target-lock" corner brackets around a rectangular area.
// Absolute coordinates relative to `parent` — pass the screen and the
// panel's own on-screen x/y/w/h.
void add_corner_brackets(lv_obj_t *parent, lv_coord_t ox, lv_coord_t oy,
                          lv_coord_t w, lv_coord_t h, lv_color_t color);

// Visual confirmation that a tap actually registered — a background
// color that only applies while LV_STATE_PRESSED is active.
void add_press_feedback(lv_obj_t *obj, lv_color_t pressed_color);

// Soft "breathing" opacity pulse, infinitely repeating — used on
// status dots/icons and glow elements for a more alive feel.
void start_pulse(lv_obj_t *obj, uint32_t half_cycle_ms);

// Slow color-cycling glow, used on the accent line under the title bar.
void start_accent_glow(lv_obj_t *obj);

// One-shot "this card just got fresh data" border flash — bright,
// fading back to normal. Not a repeating animation.
void flash_border(lv_obj_t *obj);