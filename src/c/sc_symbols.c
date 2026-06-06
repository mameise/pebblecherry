/*
 * sc_symbols.c — gezeichnete Super-Cherry-Symbole (GPath/Primitive).
 */
#include "sc_symbols.h"

#ifdef PBL_COLOR
  #define C_CHERRY   GColorRed
  #define C_CHERRY_S GColorDarkGreen
  #define C_LEMON    GColorYellow
  #define C_ORANGE   GColorOrange
  #define C_PLUM     GColorPurple
  #define C_GRAPE    GColorVividViolet
  #define C_PEAR     GColorKellyGreen
  #define C_MELON    GColorGreen
  #define C_MELON_IN GColorRed
  #define C_BELL     GColorChromeYellow
  #define C_BAR      GColorWhite
  #define C_BAR_BG   GColorBlue
  #define C_STAR     GColorYellow
  #define C_LEAF     GColorKellyGreen
  #define C_OUTLINE  GColorBlack
  #define C_SHINE    GColorWhite
#else
  #define C_CHERRY   GColorWhite
  #define C_CHERRY_S GColorWhite
  #define C_LEMON    GColorWhite
  #define C_ORANGE   GColorWhite
  #define C_PLUM     GColorWhite
  #define C_GRAPE    GColorWhite
  #define C_PEAR     GColorWhite
  #define C_MELON    GColorWhite
  #define C_MELON_IN GColorLightGray
  #define C_BELL     GColorWhite
  #define C_BAR      GColorBlack
  #define C_BAR_BG   GColorWhite
  #define C_STAR     GColorWhite
  #define C_LEAF     GColorWhite
  #define C_OUTLINE  GColorWhite
  #define C_SHINE    GColorWhite
#endif

static GPoint center_of(GRect c) {
    return GPoint(c.origin.x + c.size.w/2, c.origin.y + c.size.h/2);
}
static void fill_circle(GContext *ctx, GColor col, GPoint p, int r) {
    graphics_context_set_fill_color(ctx, col);
    graphics_fill_circle(ctx, p, r);
}
static void ring(GContext *ctx, GColor col, GPoint p, int r) {
    graphics_context_set_stroke_color(ctx, col);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_circle(ctx, p, r);
}

static void draw_round_fruit(GContext *ctx, GRect cell, GColor body) {
    GPoint c = center_of(cell);
    int rad = (cell.size.w < cell.size.h ? cell.size.w : cell.size.h)/2 - 2;
    if (rad < 4) rad = 4;
    fill_circle(ctx, body, c, rad);
    ring(ctx, C_OUTLINE, c, rad);
    graphics_context_set_stroke_color(ctx, C_CHERRY_S);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, GPoint(c.x, c.y - rad), GPoint(c.x + 2, c.y - rad - 3));
    graphics_context_set_fill_color(ctx, C_LEAF);
    graphics_fill_circle(ctx, GPoint(c.x + 4, c.y - rad - 2), 2);
    fill_circle(ctx, C_SHINE, GPoint(c.x - rad/2, c.y - rad/2), 2);
}

static void draw_pear(GContext *ctx, GRect cell, GColor body) {
    GPoint c = center_of(cell);
    int rad = cell.size.w/2 - 4;
    if (rad < 4) rad = 4;
    GPoint bottom = GPoint(c.x, c.y + rad/3);
    GPoint top    = GPoint(c.x, c.y - rad/3);
    fill_circle(ctx, body, bottom, rad);
    fill_circle(ctx, body, top, rad*2/3);
    ring(ctx, C_OUTLINE, bottom, rad);
    ring(ctx, C_OUTLINE, top, rad*2/3);
    graphics_context_set_fill_color(ctx, C_LEAF);
    graphics_fill_circle(ctx, GPoint(c.x + 2, c.y - rad - 1), 2);
    fill_circle(ctx, C_SHINE, GPoint(bottom.x - rad/2, bottom.y), 2);
}

static void draw_cherry(GContext *ctx, GRect cell) {
    GPoint c = center_of(cell);
    int rad = cell.size.w/5;
    if (rad < 3) rad = 3;
    GPoint l = GPoint(c.x - rad - 1, c.y + rad);
    GPoint r = GPoint(c.x + rad + 1, c.y + rad);
    GPoint top = GPoint(c.x, c.y - rad - 3);
    graphics_context_set_stroke_color(ctx, C_CHERRY_S);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_line(ctx, top, l);
    graphics_draw_line(ctx, top, r);
    graphics_context_set_fill_color(ctx, C_LEAF);
    graphics_fill_circle(ctx, GPoint(top.x + 4, top.y - 1), 3);
    fill_circle(ctx, C_CHERRY, l, rad);  ring(ctx, C_OUTLINE, l, rad);
    fill_circle(ctx, C_CHERRY, r, rad);  ring(ctx, C_OUTLINE, r, rad);
    fill_circle(ctx, C_SHINE, GPoint(l.x - rad/3, l.y - rad/3), 1);
    fill_circle(ctx, C_SHINE, GPoint(r.x - rad/3, r.y - rad/3), 1);
}

static void draw_bell(GContext *ctx, GRect cell) {
    GPoint c = center_of(cell);
    int w = cell.size.w/2 - 2, h = cell.size.h/2 - 2;
    if (w < 5) w = 5;
    if (h < 5) h = 5;
    graphics_context_set_fill_color(ctx, C_BELL);
    GRect body = GRect(c.x - w/2, c.y - h/2, w, h);
    graphics_fill_rect(ctx, body, 4, GCornersTop);
    graphics_fill_rect(ctx, GRect(c.x - w/2 - 2, c.y + h/2 - 2, w + 4, 3), 1, GCornersBottom);
    graphics_context_set_stroke_color(ctx, C_OUTLINE);
    graphics_draw_rect(ctx, body);
    fill_circle(ctx, C_OUTLINE, GPoint(c.x, c.y + h/2 + 2), 2);
}

static void draw_bar(GContext *ctx, GRect cell) {
    GPoint c = center_of(cell);
    int w = cell.size.w - 4, h = cell.size.h/2;
    if (w < 10) w = 10;
    if (h < 8) h = 8;
    GRect plate = GRect(c.x - w/2, c.y - h/2, w, h);
    graphics_context_set_fill_color(ctx, C_BAR_BG);
    graphics_fill_rect(ctx, plate, 3, GCornersAll);
    graphics_context_set_stroke_color(ctx, C_OUTLINE);
    graphics_draw_rect(ctx, plate);
    graphics_context_set_text_color(ctx, C_BAR);
    graphics_draw_text(ctx, "BAR",
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(plate.origin.x, plate.origin.y - 4, plate.size.w, plate.size.h + 4),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_star(GContext *ctx, GRect cell) {
    GPoint c = center_of(cell);
    int R = (cell.size.w < cell.size.h ? cell.size.w : cell.size.h)/2 - 1;
    if (R < 6) R = 6;
    int r = R / 2;
    static const int16_t cos1000[10] = {    0,  588,  951,  951,  588,
                                             0, -588, -951, -951, -588 };
    static const int16_t sin1000[10] = {-1000, -809, -309,  309,  809,
                                          1000,  809,  309, -309, -809 };
    GPoint pts[10];
    for (int i = 0; i < 10; i++) {
        int rad = (i % 2 == 0) ? R : r;
        pts[i].x = c.x + (int16_t)((cos1000[i] * rad) / 1000);
        pts[i].y = c.y + (int16_t)((sin1000[i] * rad) / 1000);
    }
    GPathInfo info = { .num_points = 10, .points = pts };
    GPath *path = gpath_create(&info);
    graphics_context_set_fill_color(ctx, C_STAR);
    gpath_draw_filled(ctx, path);
    graphics_context_set_stroke_color(ctx, C_OUTLINE);
    gpath_draw_outline(ctx, path);
    gpath_destroy(path);
}

static void draw_grape(GContext *ctx, GRect cell) {
    GPoint c = center_of(cell);
    int r = cell.size.w/8;
    if (r < 2) r = 2;
    int sx = c.x, sy = c.y - cell.size.h/4;
    GPoint pts[6] = {
        {sx, sy}, {sx - 2*r, sy + 2*r}, {sx + 2*r, sy + 2*r},
        {sx - r, sy + 2*r}, {sx + r, sy + 2*r}, {sx, sy + 4*r}
    };
    for (int i = 0; i < 6; i++) {
        fill_circle(ctx, C_GRAPE, pts[i], r);
        ring(ctx, C_OUTLINE, pts[i], r);
    }
    graphics_context_set_fill_color(ctx, C_LEAF);
    graphics_fill_circle(ctx, GPoint(sx + r, sy - r), 2);
}

static void draw_melon(GContext *ctx, GRect cell) {
    GPoint c = center_of(cell);
    int rad = cell.size.w/2 - 3;
    if (rad < 5) rad = 5;
    fill_circle(ctx, C_MELON, c, rad);
    ring(ctx, C_OUTLINE, c, rad);
    fill_circle(ctx, C_MELON_IN, c, rad - 3);
    graphics_context_set_fill_color(ctx, C_OUTLINE);
    graphics_fill_circle(ctx, GPoint(c.x - 2, c.y), 1);
    graphics_fill_circle(ctx, GPoint(c.x + 3, c.y - 2), 1);
}

void sc_draw_symbol(GContext *ctx, Symbol s, GRect cell) {
    switch (s) {
        case SYM_CHERRY: draw_cherry(ctx, cell); break;
        case SYM_BELL:   draw_bell(ctx, cell);   break;
        case SYM_BAR:    draw_bar(ctx, cell);    break;
        case SYM_STAR:   draw_star(ctx, cell);   break;
        case SYM_GRAPE:  draw_grape(ctx, cell);  break;
        case SYM_MELON:  draw_melon(ctx, cell);  break;
        case SYM_PEAR:   draw_pear(ctx, cell, C_PEAR);   break;
        case SYM_LEMON:  draw_round_fruit(ctx, cell, C_LEMON);  break;
        case SYM_ORANGE: draw_round_fruit(ctx, cell, C_ORANGE); break;
        case SYM_PLUM:   draw_round_fruit(ctx, cell, C_PLUM);   break;
        default: break;
    }
}
