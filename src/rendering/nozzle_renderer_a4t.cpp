// Copyright (C) 2025-2026 356C LLC
// SPDX-License-Identifier: GPL-3.0-or-later
/// @file nozzle_renderer_a4t.cpp
/// @brief Armored Turtle (A4T) toolhead renderer implementation
///
/// Vector-drawn A4T print head using LVGL polygon primitives.
/// Distinctive dark body with green hexagonal honeycomb accents.

#include "nozzle_renderer_a4t.h"

#include "nozzle_renderer_common.h"

#include <cmath>

// ============================================================================
// Design space: 1000x1000 centered at (500, 500)
// Mapped from A4T SVG (viewBox 0 0 766 1021)
// ============================================================================

static constexpr int DESIGN_CENTER_X = 500;
static constexpr int DESIGN_CENTER_Y = 500;

// A4T signature green palette
static constexpr uint32_t A4T_GREEN_BRIGHT = 0xBFBB4B; // Brightest accent
static constexpr uint32_t A4T_GREEN_MID    = 0x9D9830; // Mid-tone honeycomb
static constexpr uint32_t A4T_GREEN_DARK   = 0x807B29; // Dark honeycomb on extruder

// Body colors
static constexpr uint32_t BODY_DARK        = 0x292929; // Main body
static constexpr uint32_t BODY_EXTRUDER    = 0x2D2C2C; // Extruder section
static constexpr uint32_t BODY_DETAIL      = 0x5F5E5F; // Lighter detail on extruder
static constexpr uint32_t BODY_SIDE_LEFT   = 0x353435; // Left side panel
static constexpr uint32_t BODY_SIDE_RIGHT  = 0x323232; // Right side panel
static constexpr uint32_t FAN_DARK         = 0x111111; // Fan circle
static constexpr uint32_t FOOT_DARK        = 0x131313; // Bottom feet
static constexpr uint32_t SCREW_LIGHT      = 0x8C8B8B; // Screw details

// ============================================================================
// Drawing Helpers
// ============================================================================

/// @brief Draw a filled circle using triangle fan
static void draw_circle(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius,
                         lv_color_t color, int segments = 24) {
    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.color = color;
    tri_dsc.opa = LV_OPA_COVER;

    for (int i = 0; i < segments; i++) {
        float angle1 = (float)i * 2.0f * 3.14159265f / (float)segments;
        float angle2 = (float)(i + 1) * 2.0f * 3.14159265f / (float)segments;

        tri_dsc.p[0].x = cx;
        tri_dsc.p[0].y = cy;
        tri_dsc.p[1].x = cx + (int32_t)(radius * cosf(angle1));
        tri_dsc.p[1].y = cy + (int32_t)(radius * sinf(angle1));
        tri_dsc.p[2].x = cx + (int32_t)(radius * cosf(angle2));
        tri_dsc.p[2].y = cy + (int32_t)(radius * sinf(angle2));
        lv_draw_triangle(layer, &tri_dsc);
    }
}

/// @brief Draw a filled rectangle
static void draw_rect(lv_layer_t* layer, int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                       lv_color_t color) {
    lv_draw_fill_dsc_t fill_dsc;
    lv_draw_fill_dsc_init(&fill_dsc);
    fill_dsc.color = color;
    fill_dsc.opa = LV_OPA_COVER;
    lv_area_t area = {x1, y1, x2, y2};
    lv_draw_fill(layer, &fill_dsc, &area);
}

/// @brief Draw a filled triangle
static void draw_tri(lv_layer_t* layer, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                      int32_t x2, int32_t y2, lv_color_t color) {
    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.color = color;
    tri_dsc.opa = LV_OPA_COVER;
    tri_dsc.p[0].x = x0;
    tri_dsc.p[0].y = y0;
    tri_dsc.p[1].x = x1;
    tri_dsc.p[1].y = y1;
    tri_dsc.p[2].x = x2;
    tri_dsc.p[2].y = y2;
    lv_draw_triangle(layer, &tri_dsc);
}

/// @brief Draw a pointy-top hexagon using 6 triangles from center
/// Vertices at 30, 90, 150, 210, 270, 330 degrees
static void draw_hexagon(lv_layer_t* layer, int32_t cx, int32_t cy, int32_t radius,
                          lv_color_t color) {
    if (radius < 1) return;

    lv_draw_triangle_dsc_t tri_dsc;
    lv_draw_triangle_dsc_init(&tri_dsc);
    tri_dsc.color = color;
    tri_dsc.opa = LV_OPA_COVER;

    // Precompute the 6 vertices (pointy-top: start at 30 degrees)
    static constexpr float DEG_TO_RAD = 3.14159265f / 180.0f;
    int32_t vx[6], vy[6];
    for (int i = 0; i < 6; i++) {
        float angle = (30.0f + 60.0f * i) * DEG_TO_RAD;
        vx[i] = cx + (int32_t)(radius * cosf(angle));
        vy[i] = cy + (int32_t)(radius * sinf(angle));
    }

    // Draw 6 triangles from center to each edge pair
    for (int i = 0; i < 6; i++) {
        int next = (i + 1) % 6;
        tri_dsc.p[0].x = cx;
        tri_dsc.p[0].y = cy;
        tri_dsc.p[1].x = vx[i];
        tri_dsc.p[1].y = vy[i];
        tri_dsc.p[2].x = vx[next];
        tri_dsc.p[2].y = vy[next];
        lv_draw_triangle(layer, &tri_dsc);
    }
}

/// @brief Transform design-space coordinate to screen coordinate
static inline int32_t tx(int32_t design_x, int32_t cx, float scale) {
    return cx + (int32_t)((design_x - DESIGN_CENTER_X) * scale);
}

static inline int32_t ty(int32_t design_y, int32_t cy, float scale) {
    return cy + (int32_t)((design_y - DESIGN_CENTER_Y) * scale);
}

// ============================================================================
// Main Drawing Function
// ============================================================================

void draw_nozzle_a4t(lv_layer_t* layer, int32_t cx, int32_t cy, lv_color_t filament_color,
                      int32_t scale_unit, lv_opa_t opa) {
    // Scale: design space 1000x1000, rendered into scale_unit * 10 px
    int32_t render_size = scale_unit * 10;
    float scale = (float)render_size / 1000.0f;

    // Dim helper: blend color toward black by opacity factor
    auto dim = [opa](lv_color_t c) -> lv_color_t {
        if (opa >= LV_OPA_COVER)
            return c;
        float f = (float)opa / 255.0f;
        return lv_color_make((uint8_t)(c.red * f), (uint8_t)(c.green * f), (uint8_t)(c.blue * f));
    };

    // Filament detection for nozzle tip coloring
    static constexpr uint32_t NOZZLE_UNLOADED = 0x3A3A3A;
    bool has_filament = !lv_color_eq(filament_color, lv_color_hex(NOZZLE_UNLOADED)) &&
                        !lv_color_eq(filament_color, lv_color_hex(0x808080)) &&
                        !lv_color_eq(filament_color, lv_color_black());

    // Pre-dim all body colors (body is ALWAYS dark, never changes with filament)
    lv_color_t body_color      = dim(lv_color_hex(BODY_DARK));
    lv_color_t extruder_color  = dim(lv_color_hex(BODY_EXTRUDER));
    lv_color_t detail_color    = dim(lv_color_hex(BODY_DETAIL));
    lv_color_t side_left       = dim(lv_color_hex(BODY_SIDE_LEFT));
    lv_color_t side_right      = dim(lv_color_hex(BODY_SIDE_RIGHT));
    lv_color_t fan_color       = dim(lv_color_hex(FAN_DARK));
    lv_color_t foot_color      = dim(lv_color_hex(FOOT_DARK));
    lv_color_t screw_color     = dim(lv_color_hex(SCREW_LIGHT));

    // Green accents are ALWAYS A4T green, never change with filament
    lv_color_t green_bright    = dim(lv_color_hex(A4T_GREEN_BRIGHT));
    lv_color_t green_mid       = dim(lv_color_hex(A4T_GREEN_MID));
    lv_color_t green_dark      = dim(lv_color_hex(A4T_GREEN_DARK));

    // ========================================================================
    // 1. Main body — dark rectangular lower section
    //    Design space: (249,430) to (744,934)
    // ========================================================================
    {
        int32_t bx1 = tx(249, cx, scale);
        int32_t by1 = ty(430, cy, scale);
        int32_t bx2 = tx(744, cx, scale);
        int32_t by2 = ty(934, cy, scale);

        // Main body rectangle (leave bottom corners for bevel)
        int32_t bevel_w = (int32_t)(60 * scale); // Width of bottom corner bevels
        int32_t bevel_h = (int32_t)(40 * scale); // Height of bottom corner bevels

        // Main body (above bevel line)
        draw_rect(layer, bx1, by1, bx2, by2 - bevel_h, body_color);

        // Bottom section between bevels
        draw_rect(layer, bx1 + bevel_w, by2 - bevel_h, bx2 - bevel_w, by2, body_color);

        // Bottom-left bevel triangle
        draw_tri(layer,
                 bx1, by2 - bevel_h,             // top-left of bevel zone
                 bx1 + bevel_w, by2 - bevel_h,   // top-right of bevel zone
                 bx1 + bevel_w, by2,              // bottom-right of bevel zone
                 body_color);

        // Bottom-right bevel triangle
        draw_tri(layer,
                 bx2 - bevel_w, by2 - bevel_h,   // top-left of bevel zone
                 bx2, by2 - bevel_h,              // top-right of bevel zone
                 bx2 - bevel_w, by2,              // bottom-left of bevel zone
                 body_color);

        // Subtle edge highlight on top of body
        lv_color_t body_highlight = nr_lighten(body_color, 12);
        draw_rect(layer, bx1, by1, bx2, by1 + LV_MAX((int32_t)(3 * scale), 1), body_highlight);
    }

    // ========================================================================
    // 2. Extruder section — narrower rectangle on top of body
    //    Design space: (380,50) to (610,430)
    // ========================================================================
    {
        int32_t ex1 = tx(370, cx, scale);
        int32_t ey1 = ty(80, cy, scale);
        int32_t ex2 = tx(620, cx, scale);
        int32_t ey2 = ty(430, cy, scale);

        // Main extruder body
        draw_rect(layer, ex1, ey1, ex2, ey2, extruder_color);

        // Lighter detail on left side of extruder
        int32_t detail_w = (int32_t)(60 * scale);
        int32_t detail_top = ty(114, cy, scale);
        int32_t detail_bot = ty(250, cy, scale);
        draw_rect(layer, ex1, detail_top, ex1 + detail_w, detail_bot, detail_color);

        // Subtle top edge highlight
        lv_color_t ext_highlight = nr_lighten(extruder_color, 15);
        draw_rect(layer, ex1, ey1, ex2, ey1 + LV_MAX((int32_t)(2 * scale), 1), ext_highlight);
    }

    // ========================================================================
    // 3. Green honeycomb accents — A4T SIGNATURE FEATURE
    // ========================================================================
    {
        // Main honeycomb band across body at y~410-475
        // Hex radius scales with render size, minimum 2px for small scales
        int32_t hex_r = LV_MAX((int32_t)(32 * scale), 2);
        // Horizontal spacing between hex centers: radius * sqrt(3) for tight packing
        int32_t hex_dx = LV_MAX((int32_t)(hex_r * 1.732f), 3);
        // Vertical spacing between rows: radius * 1.5
        int32_t hex_dy = LV_MAX((int32_t)(hex_r * 1.5f), 2);

        // Band center Y in design space ~445
        int32_t band_cy = ty(445, cy, scale);

        // Two rows of hexagons across the body width
        // Row 1 (top row, offset by half dx for honeycomb pattern)
        int32_t row1_y = band_cy - hex_dy / 2;
        int32_t row2_y = band_cy + hex_dy / 2;

        // Band horizontal extents in design space: ~310 to ~680
        int32_t band_left = tx(310, cx, scale);
        int32_t band_right = tx(680, cx, scale);

        // Draw top row of hexagons
        for (int32_t hx = band_left + hex_dx / 2; hx <= band_right; hx += hex_dx) {
            draw_hexagon(layer, hx, row1_y, hex_r, green_bright);
        }

        // Draw bottom row (offset by half spacing for honeycomb pattern)
        for (int32_t hx = band_left + hex_dx; hx <= band_right - hex_dx / 2; hx += hex_dx) {
            draw_hexagon(layer, hx, row2_y, hex_r, green_bright);
        }

        // Darker green hexagons on extruder right side (y~160-310)
        int32_t ext_hex_r = LV_MAX((int32_t)(24 * scale), 2);
        int32_t ext_hex_dx = LV_MAX((int32_t)(ext_hex_r * 1.732f), 3);
        int32_t ext_hex_dy = LV_MAX((int32_t)(ext_hex_r * 1.5f), 2);

        int32_t ext_left = tx(430, cx, scale);
        int32_t ext_right = tx(580, cx, scale);
        int32_t ext_top = ty(160, cy, scale);
        int32_t ext_bot = ty(310, cy, scale);

        // Fill extruder accent area with darker green hexagons
        bool offset_row = false;
        for (int32_t hy = ext_top + ext_hex_r; hy <= ext_bot - ext_hex_r; hy += ext_hex_dy) {
            int32_t x_start = ext_left + ext_hex_dx / 2 + (offset_row ? ext_hex_dx / 2 : 0);
            for (int32_t hx = x_start; hx <= ext_right - ext_hex_dx / 2; hx += ext_hex_dx) {
                draw_hexagon(layer, hx, hy, ext_hex_r, green_dark);
            }
            offset_row = !offset_row;
        }

        // A bright green accent bar just above the honeycomb band
        int32_t accent_y1 = ty(418, cy, scale);
        int32_t accent_y2 = ty(432, cy, scale);
        int32_t accent_x1 = tx(280, cx, scale);
        int32_t accent_x2 = tx(510, cx, scale);
        draw_rect(layer, accent_x1, accent_y1, accent_x2, accent_y2, green_mid);
    }

    // ========================================================================
    // 4. Fan circle — dark circle in center of body
    //    Design space center: (496, 640), radius ~65
    // ========================================================================
    {
        int32_t fan_cx = tx(496, cx, scale);
        int32_t fan_cy = ty(640, cy, scale);
        int32_t fan_r = LV_MAX((int32_t)(65 * scale), 2);

        draw_circle(layer, fan_cx, fan_cy, fan_r, fan_color, 24);

        // Inner hub ring (slightly lighter)
        int32_t hub_r = LV_MAX(fan_r / 3, 1);
        draw_circle(layer, fan_cx, fan_cy, hub_r, nr_lighten(fan_color, 15), 16);
    }

    // ========================================================================
    // 5. Side panels — subtle darker strips for depth
    // ========================================================================
    {
        // Left side panel
        int32_t lp_x1 = tx(128, cx, scale);
        int32_t lp_x2 = tx(168, cx, scale);
        int32_t lp_y1 = ty(521, cy, scale);
        int32_t lp_y2 = ty(871, cy, scale);
        draw_rect(layer, lp_x1, lp_y1, lp_x2, lp_y2, side_left);

        // Right side panel
        int32_t rp_x1 = tx(598, cx, scale);
        int32_t rp_x2 = tx(671, cx, scale);
        int32_t rp_y1 = ty(496, cy, scale);
        int32_t rp_y2 = ty(816, cy, scale);
        draw_rect(layer, rp_x1, rp_y1, rp_x2, rp_y2, side_right);
    }

    // ========================================================================
    // 6. Bottom feet — darker sections at bottom corners
    // ========================================================================
    {
        // Left foot
        int32_t lf_x1 = tx(98, cx, scale);
        int32_t lf_x2 = tx(287, cx, scale);
        int32_t lf_y1 = ty(894, cy, scale);
        int32_t lf_y2 = ty(1002, cy, scale);
        draw_rect(layer, lf_x1, lf_y1, lf_x2, lf_y2, foot_color);

        // Right foot
        int32_t rf_x1 = tx(482, cx, scale);
        int32_t rf_x2 = tx(677, cx, scale);
        int32_t rf_y1 = ty(895, cy, scale);
        int32_t rf_y2 = ty(1002, cy, scale);
        draw_rect(layer, rf_x1, rf_y1, rf_x2, rf_y2, foot_color);
    }

    // ========================================================================
    // 7. Small screw details for visual interest at larger scales
    // ========================================================================
    if (scale > 0.04f) {
        int32_t screw_r = LV_MAX((int32_t)(8 * scale), 1);
        // Two screws on the extruder section
        draw_circle(layer, tx(390, cx, scale), ty(140, cy, scale), screw_r, screw_color, 12);
        draw_circle(layer, tx(390, cx, scale), ty(220, cy, scale), screw_r, screw_color, 12);
        // Two screws on the body
        draw_circle(layer, tx(300, cx, scale), ty(520, cy, scale), screw_r, screw_color, 12);
        draw_circle(layer, tx(690, cx, scale), ty(520, cy, scale), screw_r, screw_color, 12);
    }

    // ========================================================================
    // 8. Nozzle tip — shows filament color when loaded
    //    Positioned below body bottom (~Y=950 in design space)
    // ========================================================================
    {
        lv_color_t tip_color = dim(filament_color);
        lv_color_t nozzle_metal = dim(lv_color_hex(NOZZLE_UNLOADED));

        int32_t nozzle_top_y = ty(940, cy, scale);
        int32_t nozzle_height = LV_MAX((int32_t)(40 * scale), 2);
        int32_t nozzle_top_width = LV_MAX((int32_t)(70 * scale), 4);
        int32_t nozzle_bottom_width = LV_MAX((int32_t)(24 * scale), 2);

        lv_color_t tip_left = has_filament ? nr_lighten(tip_color, 30) : nr_lighten(nozzle_metal, 30);
        lv_color_t tip_right = has_filament ? nr_darken(tip_color, 20) : nr_darken(nozzle_metal, 10);

        nr_draw_nozzle_tip(layer, cx, nozzle_top_y, nozzle_top_width, nozzle_bottom_width,
                           nozzle_height, tip_left, tip_right);

        // Bright glint at tip bottom
        lv_draw_fill_dsc_t glint_dsc;
        lv_draw_fill_dsc_init(&glint_dsc);
        glint_dsc.color = dim(lv_color_hex(0xFFFFFF));
        glint_dsc.opa = LV_OPA_70;
        int32_t glint_y = nozzle_top_y + nozzle_height - 1;
        lv_area_t glint = {cx - 1, glint_y, cx + 1, glint_y + 1};
        lv_draw_fill(layer, &glint_dsc, &glint);
    }
}
