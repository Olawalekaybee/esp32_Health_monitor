#pragma once
#include <lvgl.h>
// ============================================================
// status_icons.h — animated icon badges for WiFi/SD/Cloud/comms
// transport, all stacked in one left-side panel.
//
// Honest framing on "3D": this hardware has no 3D renderer. These are
// real LVGL symbol glyphs (LV_SYMBOL_WIFI, LV_SYMBOL_SD_CARD,
// LV_SYMBOL_UPLOAD — all verified present in this project's actual
// lv_symbol_def.h, not guessed) with a colored glow and a continuous
// pulse animation behind them for a sense of life and depth — a
// stylized 2D treatment, not real 3D geometry.
// ============================================================

// One flex-column container that stacks whatever badges are added to
// it — LVGL computes each child's position; nothing here is
// hand-calculated x/y math (a previous hand-calculated version of
// this produced a badge that rendered far bigger and in the wrong
// place than intended).
lv_obj_t *make_side_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t panel_h);

// Adds one icon badge to `panel`. `symbol` is one of the LV_SYMBOL_*
// constants. Returns the glyph label itself — pass that to
// set_icon_status_color() to recolor it (and its glow) based on live
// status. out_badge is optional, needed only when the caller also
// wants to make the whole badge tappable (the Cloud badge, for
// force-sync).
lv_obj_t *make_status_icon_badge(lv_obj_t *panel, const char *symbol,
                                   lv_obj_t **out_badge = nullptr);

// Recolors a status icon's glyph and its badge's glow/border together,
// so all three always match (e.g. all green when healthy, all red
// when not) — mirrors set_dot_color()'s "fill + glow together" pattern
// from ui_common.h, just for a glyph-based badge instead of a plain dot.
void set_icon_status_color(lv_obj_t *glyph, lv_color_t color);