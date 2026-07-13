#include "status_icons.h"
#include "ui_common.h"
#include "theme.h"

lv_obj_t *make_side_panel(lv_obj_t *parent, lv_coord_t x, lv_coord_t panel_h) {
    lv_obj_t *panel = lv_obj_create(parent);
    bare(panel);
    lv_obj_set_size(panel, PANEL_W, panel_h);
    lv_obj_set_pos(panel, x, TITLE_BAR_H + ACCENT_LINE_H);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0); // invisible container — only the badges inside it are visible
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);  // flex containers are scrollable by default; not wanted here
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return panel;
}

lv_obj_t *make_status_icon_badge(lv_obj_t *panel, const char *symbol, lv_obj_t **out_badge) {
    lv_obj_t *badge = lv_obj_create(panel);
    bare(badge);
    lv_obj_set_size(badge, PANEL_W - 6, 44);
    lv_obj_set_style_bg_color(badge, lv_color_hex(0x151b24), 0);
    lv_obj_set_style_bg_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(badge, 10, 0);
    lv_obj_set_style_border_width(badge, 1, 0);
    lv_obj_set_style_border_color(badge, COLOR_CARD_BORDER, 0);
    // Glow starts invisible (LV_OPA_0) — set_icon_status_color() below
    // brings it up to LV_OPA_50 once a real status color is known, so
    // the badge doesn't glow an arbitrary color before the first
    // real packet arrives.
    lv_obj_set_style_shadow_width(badge, 10, 0);
    lv_obj_set_style_shadow_opa(badge, LV_OPA_0, 0);
    start_pulse(badge, 1000);

    lv_obj_t *glyph = lv_label_create(badge);
    lv_label_set_text(glyph, symbol);
    lv_obj_set_style_text_font(glyph, FONT_LABEL, 0);
    lv_obj_set_style_text_color(glyph, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_center(glyph);

    if (out_badge) {
        *out_badge = badge;
    }
    return glyph;
}

void set_icon_status_color(lv_obj_t *glyph, lv_color_t color) {
    lv_obj_set_style_text_color(glyph, color, 0);

    lv_obj_t *badge = lv_obj_get_parent(glyph);
    lv_obj_set_style_shadow_color(badge, color, 0);
    lv_obj_set_style_shadow_opa(badge, LV_OPA_50, 0);
    lv_obj_set_style_border_color(badge, color, 0);
}