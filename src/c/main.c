/*
 * main.c — Super Cherry fuer Pebble (emery / basalt / chalk / diorite / aplite)
 *
 * Echte Super-Cherry-Mechanik mit gezeichneten Symbolen.
 *
 * Ablauf wie beim echten Automaten:
 *   1. SELECT startet Spin.
 *   2. Bei 2 gleichen loest das Geraet ZUFAELLIG Hold oder Step aus
 *      (automatisch, keine Spielerwahl) und animiert die 3. Walze.
 *   3. Bei Gewinn erscheint das Gamble-Menue:
 *        UP    = Gamble 2x
 *        SELECT= Gewinn nehmen
 *        DOWN  = Gamble Mystery
 *        UP lang = Gamble Split
 *      Im 2x-Aufdecken: UP/DOWN = linke/rechte Seite tippen.
 *   4. 3 Sterne -> Star Feature laeuft automatisch (SELECT spielt es aus).
 *
 * Bedienkern (3 Tasten) ist an PiTama angelehnt.
 */
#include <pebble.h>
#include "sc_core.h"
#include "sc_symbols.h"

static Window  *s_window;
static Layer   *s_canvas;
static SCGame   s_game;

static AppTimer *s_timer;
static int       s_anim_frames;
static bool      s_animating;
static char      s_status[64];
static char      s_title[24];

#ifdef PBL_COLOR
  #define BG       GColorBlack
  #define FRAME    GColorRed
  #define LINE     GColorYellow
  #define TXT      GColorWhite
  #define WINCOL   GColorGreen
  #define ACCENT   GColorChromeYellow
#else
  #define BG       GColorBlack
  #define FRAME    GColorWhite
  #define LINE     GColorWhite
  #define TXT      GColorWhite
  #define WINCOL   GColorWhite
  #define ACCENT   GColorWhite
#endif

static void set_status(const char *t){ snprintf(s_status,sizeof(s_status),"%s",t); }

/* ============ Zeichnen ============ */
static void draw_reels(GContext *ctx, GRect b) {
    int cell = (b.size.w - 24) / 3;
    if (cell > 46) cell = 46;
    int grid_w = cell*3 + 8;
    int x0 = (b.size.w - grid_w)/2 + 2;
    int y0 = 30;
    int grid_h = cell*3 + 8;

    /* Gehaeuse-Rahmen */
    graphics_context_set_stroke_color(ctx, FRAME);
    graphics_context_set_stroke_width(ctx, 3);
    graphics_draw_round_rect(ctx, GRect(x0-6, y0-6, grid_w+8, grid_h+8), 4);

    /* Gewinnlinie (mittlere Reihe) markieren */
    int line_y = y0 + cell + 4 + cell/2;
    graphics_context_set_stroke_color(ctx, LINE);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(x0-10, line_y), GPoint(x0+grid_w, line_y));
    /* Pfeile links/rechts */
    graphics_context_set_fill_color(ctx, LINE);
    graphics_fill_circle(ctx, GPoint(x0-9, line_y), 3);
    graphics_fill_circle(ctx, GPoint(x0+grid_w-1, line_y), 3);

    for (int r=0;r<SC_REELS;r++){
        int cx = x0 + r*(cell+4);
        for (int row=0;row<3;row++){
            int cy = y0 + row*(cell+4);
            GRect c = GRect(cx, cy, cell, cell);
            /* Zellenhintergrund leicht abgesetzt fuer die Gewinnlinie */
            if (row==1){
                graphics_context_set_fill_color(ctx, GColorDarkGray);
                graphics_fill_rect(ctx, c, 3, GCornersAll);
            }
            sc_draw_symbol(ctx, s_game.window[r][row], c);
        }
    }
}

static void draw_hud(GContext *ctx, GRect b) {
    /* Titel */
    graphics_context_set_text_color(ctx, ACCENT);
    graphics_draw_text(ctx, s_title,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(0,0,b.size.w,20), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    /* Credits / Einsatz */
    char info[40];
    snprintf(info,sizeof(info),"CR %d   EINSATZ %d",(int)s_game.credits,(int)s_game.bet);
    graphics_context_set_text_color(ctx, TXT);
    graphics_draw_text(ctx, info,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0, b.size.h-44, b.size.w, 16),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    /* Statuszeile */
    GColor sc = (s_game.last_win>0)?WINCOL:TXT;
    graphics_context_set_text_color(ctx, sc);
    graphics_draw_text(ctx, s_status,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(2, b.size.h-28, b.size.w-4, 26),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_gamble_menu(GContext *ctx, GRect b) {
    char w[40];
    snprintf(w,sizeof(w),"GEWINN: %d",(int)s_game.last_win);
    graphics_context_set_text_color(ctx, WINCOL);
    graphics_draw_text(ctx, w, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(0,28,b.size.w,32), GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, TXT);
    graphics_draw_text(ctx,
        "UP  = Gamble 2x\n"
        "DOWN= Mystery x?\n"
        "UP lang = Split\n"
        "SELECT = nehmen",
        fonts_get_system_font(FONT_KEY_GOTHIC_18),
        GRect(6, 70, b.size.w-12, 110),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_gamble_2x(GContext *ctx, GRect b) {
    char w[40]; snprintf(w,sizeof(w),"RISIKO: %d",(int)s_game.last_win);
    graphics_context_set_text_color(ctx, ACCENT);
    graphics_draw_text(ctx, w, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(0,34,b.size.w,32), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    /* zwei Felder, eines gewinnt */
    int fw = b.size.w/2 - 20;
    graphics_context_set_fill_color(ctx, GColorDarkGray);
    graphics_fill_rect(ctx, GRect(15, 90, fw, 50), 6, GCornersAll);
    graphics_fill_rect(ctx, GRect(b.size.w-15-fw, 90, fw, 50), 6, GCornersAll);
    graphics_context_set_text_color(ctx, TXT);
    graphics_draw_text(ctx, "UP", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(15, 100, fw, 30), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_draw_text(ctx, "DOWN", fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(b.size.w-15-fw, 100, fw, 30), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, TXT);
    graphics_draw_text(ctx, "Tippe ein Feld - 50/50",
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(0,150,b.size.w,18), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_star(GContext *ctx, GRect b) {
    /* grosser Stern + Aufforderung */
    GRect big = GRect(b.size.w/2-30, 36, 60, 60);
    sc_draw_symbol(ctx, SYM_STAR, big);
    graphics_context_set_text_color(ctx, ACCENT);
    graphics_draw_text(ctx, "STAR FEATURE!",
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(0,100,b.size.w,28), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    graphics_context_set_text_color(ctx, TXT);
    graphics_draw_text(ctx, "SELECT = Bonus spielen",
        fonts_get_system_font(FONT_KEY_GOTHIC_18),
        GRect(0,135,b.size.w,24), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void canvas_update(Layer *layer, GContext *ctx) {
    GRect b = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, BG);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    switch (s_game.phase) {
        case PHASE_GAMBLE:    draw_gamble_menu(ctx, b); break;
        case PHASE_GAMBLE_2X: draw_gamble_2x(ctx, b);   break;
        case PHASE_STAR:      draw_star(ctx, b);        break;
        default:              draw_reels(ctx, b);       break;
    }
    draw_hud(ctx, b);
}

/* ============ Phasen-Uebergaenge nach Animation ============ */
static void announce_result(void) {
    if (s_game.cherry_step) {
        set_status("CHERRY STEP! 3 Kirschen");
    }
    if (s_game.last_win > 0) {
        char w[40]; snprintf(w,sizeof(w),"GEWINN %d - gamble?",(int)s_game.last_win);
        set_status(w);
        vibes_short_pulse();
    } else if (s_game.star_pending) {
        set_status("3 Sterne! Star Feature");
        vibes_double_pulse();
    } else {
        set_status("Kein Glueck - SELECT");
    }
}

/* Step-Animation: ein Nudge pro Frame */
static void step_tick(void *d);
static void start_step_anim(void){
    s_animating = true;
    s_timer = app_timer_register(220, step_tick, NULL);
}
static void step_tick(void *d){
    (void)d;
    bool more = sc_step_advance(&s_game);
    layer_mark_dirty(s_canvas);
    if (more) {
        s_timer = app_timer_register(220, step_tick, NULL);
    } else {
        s_animating = false;
        announce_result();
        layer_mark_dirty(s_canvas);
    }
}

/* Hold-Animation: kurzes Wirbeln der dritten Walze, dann aufloesen */
static void hold_tick(void *d);
static void start_hold_anim(void){
    s_animating = true;
    s_anim_frames = 8;
    s_timer = app_timer_register(60, hold_tick, NULL);
}
static void hold_tick(void *d){
    (void)d;
    if (s_anim_frames > 0){
        /* abweichende Walze optisch durchwirbeln */
        for (int r=0;r<SC_REELS;r++){
            /* nur die Walze, die nicht zum Paar gehoert, wirbeln:
               einfache Heuristik - wirbeln aller, Ergebnis wird restauriert */
        }
        /* Wir wirbeln alle Reihen der dritten sichtbaren Walze visuell. */
        for (int row=0;row<3;row++){
            s_game.window[2][row] = (Symbol)((s_game.window[2][row]+1)%SYM_COUNT);
        }
        s_anim_frames--;
        layer_mark_dirty(s_canvas);
        s_timer = app_timer_register(60, hold_tick, NULL);
        return;
    }
    sc_resolve_hold(&s_game);
    s_animating = false;
    announce_result();
    layer_mark_dirty(s_canvas);
}

/* Spin-Animation */
static void spin_tick(void *d);
static void start_spin_anim(void){
    s_animating = true;
    s_anim_frames = 12;
    s_timer = app_timer_register(45, spin_tick, NULL);
}
static void spin_tick(void *d){
    (void)d;
    if (s_anim_frames > 0){
        for (int r=0;r<SC_REELS;r++)
            for (int row=0;row<3;row++)
                s_game.window[r][row] = (Symbol)((s_game.window[r][row]+1+r)%SYM_COUNT);
        s_anim_frames--;
        layer_mark_dirty(s_canvas);
        s_timer = app_timer_register(45 + (12-s_anim_frames)*5, spin_tick, NULL);
        return;
    }
    sc_restore_result(&s_game);   /* echtes Spin-Ergebnis */
    s_animating = false;

    sc_after_spin(&s_game);       /* entscheidet Hold/Step/Direkt */

    if (s_game.phase == PHASE_HOLD){
        set_status("HOLD - Walze dreht neu");
        start_hold_anim();
    } else if (s_game.phase == PHASE_STEP){
        set_status(s_game.feature==FEATURE_STEP ? "STEP - Walze rueckt" : "STEP");
        start_step_anim();
    } else {
        announce_result();
        layer_mark_dirty(s_canvas);
    }
}

static void do_spin(void){
    if (s_animating) return;
    if (!sc_spin(&s_game)){ set_status("Zu wenig Credits!"); layer_mark_dirty(s_canvas); return; }
    sc_save_result(&s_game);
    set_status("...dreht...");
    snprintf(s_title,sizeof(s_title),"SUPER CHERRY");
    start_spin_anim();
}

/* ============ Buttons ============ */
static void select_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    if (s_animating) return;
    switch (s_game.phase){
        case PHASE_IDLE:   do_spin(); break;
        case PHASE_GAMBLE: {
            int32_t w = s_game.last_win;
            sc_gamble_pick(&s_game, GAMBLE_PICK_TAKE);
            char m[40]; snprintf(m,sizeof(m),"genommen: %d",(int)w); set_status(m);
            break;
        }
        case PHASE_STAR: {
            int32_t w = sc_play_star(&s_game);
            char m[48];
            const char* names[]={"Fruit Stop","2x Shuffle","4x Shuffle","10x","Cherry Collect"};
            snprintf(m,sizeof(m),"%s: +%d",names[s_game.star_game],(int)w);
            set_status(m); vibes_short_pulse();
            break;
        }
        default: break;
    }
    layer_mark_dirty(s_canvas);
}

static void up_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    if (s_animating) return;
    switch (s_game.phase){
        case PHASE_IDLE:
            /* nichts (Einsatz aendert man mit DOWN) */
            break;
        case PHASE_GAMBLE:
            sc_gamble_pick(&s_game, GAMBLE_PICK_2X);
            set_status("2x: tippe UP oder DOWN");
            break;
        case PHASE_GAMBLE_2X: {
            bool won = sc_gamble_2x_reveal(&s_game, 0);
            set_status(won ? "GEWONNEN!" : "verloren...");
            if (won) vibes_short_pulse(); else vibes_double_pulse();
            break;
        }
        default: break;
    }
    layer_mark_dirty(s_canvas);
}

static void up_long_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    if (s_animating) return;
    if (s_game.phase == PHASE_GAMBLE){
        sc_gamble_pick(&s_game, GAMBLE_PICK_SPLIT);
        set_status("Split: Haelfte sicher, 2x!");
        layer_mark_dirty(s_canvas);
    }
}

static void down_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    if (s_animating) return;
    switch (s_game.phase){
        case PHASE_IDLE: {
            sc_cycle_bet(&s_game);
            char m[24]; snprintf(m,sizeof(m),"Einsatz %d",(int)s_game.bet); set_status(m);
            break;
        }
        case PHASE_GAMBLE:
            sc_gamble_pick(&s_game, GAMBLE_PICK_MYSTERY);
            { int m = sc_mystery_reveal(&s_game);
              char t[40]; snprintf(t,sizeof(t),"Mystery x%d -> %d",m,(int)s_game.last_win);
              set_status(t);
              if (m>0) vibes_short_pulse(); else vibes_double_pulse(); }
            break;
        case PHASE_GAMBLE_2X: {
            bool won = sc_gamble_2x_reveal(&s_game, 1);
            set_status(won ? "GEWONNEN!" : "verloren...");
            if (won) vibes_short_pulse(); else vibes_double_pulse();
            break;
        }
        default: break;
    }
    layer_mark_dirty(s_canvas);
}

static void click_config(void *context){
    (void)context;
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
    window_single_click_subscribe(BUTTON_ID_UP, up_click);
    window_long_click_subscribe(BUTTON_ID_UP, 500, up_long_click, NULL);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

/* ============ Lifecycle ============ */
static void window_load(Window *w){
    Layer *root = window_get_root_layer(w);
    s_canvas = layer_create(layer_get_bounds(root));
    layer_set_update_proc(s_canvas, canvas_update);
    layer_add_child(root, s_canvas);
}
static void window_unload(Window *w){ (void)w; layer_destroy(s_canvas); }

static void init(void){
    sc_init(&s_game, (uint32_t)time(NULL) ^ 0x5C600u);
    snprintf(s_title,sizeof(s_title),"SUPER CHERRY");
    set_status("SELECT=Spin  DOWN=Einsatz");
    s_window = window_create();
    window_set_background_color(s_window, BG);
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers){
        .load=window_load, .unload=window_unload });
    window_stack_push(s_window, true);
}
static void deinit(void){ window_destroy(s_window); }

int main(void){ init(); app_event_loop(); deinit(); }
