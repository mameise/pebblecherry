/*
 * main.c — Super Cherry 600 fuer Pebble (emery / basalt / chalk / aplite / diorite)
 *
 * Bedienung (an PiTama-3-Tasten-Schema angelehnt):
 *   IDLE:   SELECT = Spin starten | UP = (Pot einsammeln wenn voll) | DOWN = Einsatz +
 *   HOLD:   UP/SELECT/DOWN = Walze 1/2/3 halten toggeln | langes SELECT = weiterdrehen
 *   EVAL:   SELECT = weiter (zurueck zu IDLE) | UP = ins Gamble (wenn Gewinn)
 *   GAMBLE: UP = Rot tippen | DOWN = Schwarz tippen | SELECT = Gewinn einsammeln
 *   BACK in IDLE verlaesst die App.
 *
 * Spiellogik liegt vollstaendig in sc600_core.c (plattformunabhaengig).
 */
#include <pebble.h>
#include "sc600_core.h"

static Window    *s_window;
static Layer     *s_canvas;
static SC600Game  s_game;

/* Spin-Animation */
static AppTimer  *s_spin_timer;
static int        s_spin_frames;       /* verbleibende Animationsframes */
static bool       s_animating;
static bool       s_held_once;    /* verhindert endloses Re-Spinnen */

/* Statuszeile */
static char       s_status[48];

/* ---- Farben (graceful auf S/W-Plattformen) ------------------------- */
#ifdef PBL_COLOR
  #define COL_BG        GColorBlack
  #define COL_FRAME     GColorRed
  #define COL_LINE      GColorYellow
  #define COL_TEXT      GColorWhite
  #define COL_WIN       GColorGreen
  #define COL_HOLD      GColorChromeYellow
  #define COL_CHERRY    GColorRed
#else
  #define COL_BG        GColorBlack
  #define COL_FRAME     GColorWhite
  #define COL_LINE      GColorWhite
  #define COL_TEXT      GColorWhite
  #define COL_WIN       GColorWhite
  #define COL_HOLD      GColorWhite
  #define COL_CHERRY    GColorWhite
#endif

/* ---- Symbol-Kurztext ------------------------------------------------ */
static const char* sym_label(Symbol s) {
    switch (s) {
        case SYM_CHERRY: return "CH";
        case SYM_LEMON:  return "ZI";
        case SYM_ORANGE: return "OR";
        case SYM_PLUM:   return "PF";
        case SYM_GRAPE:  return "TR";
        case SYM_MELON:  return "ME";
        case SYM_BELL:   return "GL";
        case SYM_BAR:    return "BAR";
        case SYM_SEVEN:  return "7";
        case SYM_STAR:   return "*";
        default:         return "?";
    }
}

static void set_status(const char *txt) {
    snprintf(s_status, sizeof(s_status), "%s", txt);
}

/* ---- Zeichnen ------------------------------------------------------- */
static void draw_reels(GContext *ctx, GRect bounds) {
    /* Walzenfenster mittig. 3x3 Gitter. */
    const int cell_w = bounds.size.w / 3 - 6;
    const int cell_h = 30;
    const int grid_w = cell_w * 3 + 12;
    const int grid_h = cell_h * 3 + 12;
    const int x0 = (bounds.size.w - grid_w) / 2;
    const int y0 = 34;

    /* Rahmen */
    graphics_context_set_stroke_color(ctx, COL_FRAME);
    graphics_context_set_stroke_width(ctx, 2);
    graphics_draw_rect(ctx, GRect(x0 - 2, y0 - 2, grid_w + 4, grid_h + 4));

    /* Gewinnlinie hervorheben (mittlere Reihe) */
    int line_y = y0 + cell_h + 6 + cell_h / 2;
    graphics_context_set_stroke_color(ctx, COL_LINE);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(x0 - 6, line_y), GPoint(x0 + grid_w + 4, line_y));

    for (int r = 0; r < SC_REELS; r++) {
        int cx = x0 + r * (cell_w + 6);
        for (int row = 0; row < 3; row++) {
            int cy = y0 + row * (cell_h + 6);
            GRect cell = GRect(cx, cy, cell_w, cell_h);

            /* Hold-Markierung */
            if (s_game.hold[r] && s_game.phase == PHASE_HOLD) {
                graphics_context_set_fill_color(ctx, COL_HOLD);
                graphics_fill_rect(ctx, cell, 3, GCornersAll);
            }

            Symbol sym = s_game.window[r][row];
            GColor tc = (sym == SYM_CHERRY) ? COL_CHERRY : COL_TEXT;
            if (s_game.hold[r] && s_game.phase == PHASE_HOLD) tc = GColorBlack;
            graphics_context_set_text_color(ctx, tc);

            graphics_draw_text(ctx, sym_label(sym),
                fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
                cell, GTextOverflowModeFill, GTextAlignmentCenter, NULL);
        }
    }
}

static void draw_hud(GContext *ctx, GRect bounds) {
    char top[40];
    snprintf(top, sizeof(top), "CR %d  EINS %d", (int)s_game.credits, (int)s_game.bet);
    graphics_context_set_text_color(ctx, COL_TEXT);
    graphics_draw_text(ctx, top,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(2, 2, bounds.size.w - 4, 20),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    /* Cherry-Pot-Balken unten */
    int pot_y = bounds.size.h - 44;
    char pot[24];
    snprintf(pot, sizeof(pot), "POT %d/%d", (int)s_game.cherry_pot, SC_POT_THRESHOLD);
    graphics_context_set_text_color(ctx, COL_CHERRY);
    graphics_draw_text(ctx, pot,
        fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(2, pot_y - 16, bounds.size.w - 4, 16),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    int bar_w = bounds.size.w - 20;
    int fill = (s_game.cherry_pot * bar_w) / SC_POT_THRESHOLD;
    graphics_context_set_stroke_color(ctx, COL_FRAME);
    graphics_draw_rect(ctx, GRect(10, pot_y, bar_w, 8));
    graphics_context_set_fill_color(ctx, COL_CHERRY);
    graphics_fill_rect(ctx, GRect(10, pot_y, fill, 8), 0, GCornerNone);

    /* Statuszeile ganz unten */
    GColor sc = (s_game.last_win > 0) ? COL_WIN : COL_TEXT;
    graphics_context_set_text_color(ctx, sc);
    graphics_draw_text(ctx, s_status,
        fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(2, bounds.size.h - 26, bounds.size.w - 4, 24),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_gamble(GContext *ctx, GRect bounds) {
    char msg[40];
    snprintf(msg, sizeof(msg), "GAMBLE  %d", (int)s_game.gamble_stake);
    graphics_context_set_text_color(ctx, COL_WIN);
    graphics_draw_text(ctx, msg,
        fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
        GRect(2, 50, bounds.size.w - 4, 32),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);

    graphics_context_set_text_color(ctx, COL_TEXT);
    graphics_draw_text(ctx, "UP = ROT\nDOWN = SCHWARZ\nSELECT = einsammeln",
        fonts_get_system_font(FONT_KEY_GOTHIC_18),
        GRect(4, 92, bounds.size.w - 8, 80),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void canvas_update(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, COL_BG);
    graphics_fill_rect(ctx, bounds, 0, GCornerNone);

    if (s_game.phase == PHASE_GAMBLE) {
        draw_gamble(ctx, bounds);
    } else {
        draw_reels(ctx, bounds);
    }
    draw_hud(ctx, bounds);
}

/* ---- Spin-Animation ------------------------------------------------- */
static void spin_tick(void *data);

static void start_spin_anim(void) {
    s_animating   = true;
    s_spin_frames = 12;
    s_spin_timer  = app_timer_register(40, spin_tick, NULL);
}

static void spin_tick(void *data) {
    /* Waehrend der Animation nicht-gehaltene Walzen rasch durchwirbeln. */
    if (s_spin_frames > 0) {
        for (int r = 0; r < SC_REELS; r++) {
            if (s_game.hold[r]) continue;
            for (int row = 0; row < 3; row++) {
                s_game.window[r][row] =
                    (Symbol)((s_game.window[r][row] + 1 + r) % SYM_COUNT);
            }
        }
        s_spin_frames--;
        layer_mark_dirty(s_canvas);
        s_spin_timer = app_timer_register(40 + (12 - s_spin_frames) * 4,
                                          spin_tick, NULL);
        return;
    }

    /* Animation fertig -> endgueltiges Bild war bereits vor Anim gesetzt.
     * Wir haben das echte Ergebnis schon in sc600_spin() berechnet, aber
     * durch das Durchwirbeln ueberschrieben. Daher: echtes Ergebnis wurde
     * vorher gesichert und wird jetzt restauriert. */
    extern void sc600_restore_result(SC600Game *g);
    sc600_restore_result(&s_game);

    s_animating = false;

    /* Hold anbieten: Spieler kann Walzen halten und einmal neu drehen,
     * bevor ausgewertet wird (taktisches Feature der 600er). */
    s_game.phase = PHASE_HOLD;
    set_status("HALTEN: UP/SEL/DOWN | dann SEL lang");
    layer_mark_dirty(s_canvas);
}

/* Auswertung nach Hold/Respin ausloesen. */
static void resolve_after_hold(void) {
    int32_t win = sc600_evaluate(&s_game);
    if (win > 0) {
        char w[40];
        snprintf(w, sizeof(w), "GEWINN +%d!", (int)win);
        set_status(w);
        vibes_short_pulse();
    } else if (s_game.step_armed) {
        set_status("STEP! SELECT druecken");
    } else {
        set_status("Nochmal? SELECT");
    }
    if (s_game.cherry_pot >= SC_POT_THRESHOLD) {
        set_status("CHERRY POT VOLL! UP");
        vibes_double_pulse();
    }
    layer_mark_dirty(s_canvas);
}

/* ---- Button-Handler ------------------------------------------------- */
static void do_new_spin(void) {
    if (s_animating) return;
    s_held_once = false;
    /* Ergebnis vorberechnen, dann animieren, dann restaurieren. */
    s_game.phase = PHASE_IDLE;
    if (!sc600_spin(&s_game)) {
        set_status("Zu wenig Credits!");
        layer_mark_dirty(s_canvas);
        return;
    }
    extern void sc600_save_result(SC600Game *g);
    sc600_save_result(&s_game);
    set_status("...dreht...");
    start_spin_anim();
}

static void select_click(ClickRecognizerRef rec, void *ctx) {
    switch (s_game.phase) {
        case PHASE_IDLE:
            do_new_spin();
            break;
        case PHASE_HOLD:
            sc600_toggle_hold(&s_game, 1); /* SELECT haelt mittlere Walze */
            break;
        case PHASE_EVAL:
            if (s_game.step_armed) {
                sc600_do_step(&s_game);
                int32_t w = sc600_evaluate(&s_game);
                char m[40];
                snprintf(m, sizeof(m), w > 0 ? "STEP +%d!" : "kein Step-Glueck", (int)w);
                set_status(m);
                if (w > 0) vibes_short_pulse();
            } else {
                s_game.phase = PHASE_IDLE;
                set_status("Einsatz: DOWN | Spin: SELECT");
            }
            break;
        case PHASE_GAMBLE:
            sc600_gamble_collect(&s_game);
            set_status("eingesammelt");
            break;
        default:
            break;
    }
    layer_mark_dirty(s_canvas);
}

static void select_long_click(ClickRecognizerRef rec, void *ctx) {
    if (s_game.phase != PHASE_HOLD) return;
    if (s_held_once) {
        /* Bereits einmal neu gedreht -> jetzt auswerten. */
        resolve_after_hold();
        return;
    }
    /* Pruefen ob ueberhaupt etwas gehalten wird. */
    bool any_hold = false;
    for (int r = 0; r < SC_REELS; r++) if (s_game.hold[r]) any_hold = true;

    if (!any_hold) {
        /* Nichts gehalten -> direkt auswerten. */
        resolve_after_hold();
        return;
    }
    /* Respin: gehaltene Walzen bleiben, Rest dreht neu. */
    s_held_once = true;
    s_game.phase = PHASE_HOLD; /* bleibt, sc600_spin akzeptiert HOLD */
    if (sc600_spin(&s_game)) {
        sc600_save_result(&s_game);
        set_status("...Respin...");
        start_spin_anim();
    }
}

static void up_click(ClickRecognizerRef rec, void *ctx) {
    switch (s_game.phase) {
        case PHASE_HOLD:
            sc600_toggle_hold(&s_game, 0); /* UP haelt linke Walze */
            break;
        case PHASE_IDLE:
            if (s_game.cherry_pot >= SC_POT_THRESHOLD) {
                int32_t p = sc600_collect_pot(&s_game);
                char m[40];
                snprintf(m, sizeof(m), "POT! +%d", (int)p);
                set_status(m);
                vibes_long_pulse();
            }
            break;
        case PHASE_EVAL:
            if (s_game.can_gamble) {
                s_game.phase = PHASE_GAMBLE;
                set_status("Rot oder Schwarz?");
            }
            break;
        case PHASE_GAMBLE: {
            GambleResult r = sc600_gamble(&s_game, 0); /* 0 = Rot */
            if (r == GAMBLE_WON) { set_status("GEWONNEN! weiter?"); vibes_short_pulse(); }
            else if (r == GAMBLE_LOST) { set_status("verloren..."); vibes_double_pulse(); }
            break;
        }
        default:
            break;
    }
    layer_mark_dirty(s_canvas);
}

static void down_click(ClickRecognizerRef rec, void *ctx) {
    switch (s_game.phase) {
        case PHASE_HOLD:
            sc600_toggle_hold(&s_game, 2); /* DOWN haelt rechte Walze */
            break;
        case PHASE_IDLE:
            sc600_cycle_bet(&s_game);
            { char m[40]; snprintf(m, sizeof(m), "Einsatz %d", (int)s_game.bet);
              set_status(m); }
            break;
        case PHASE_GAMBLE: {
            GambleResult r = sc600_gamble(&s_game, 1); /* 1 = Schwarz */
            if (r == GAMBLE_WON) { set_status("GEWONNEN! weiter?"); vibes_short_pulse(); }
            else if (r == GAMBLE_LOST) { set_status("verloren..."); vibes_double_pulse(); }
            break;
        }
        default:
            break;
    }
    layer_mark_dirty(s_canvas);
}

static void click_config(void *context) {
    window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
    window_long_click_subscribe(BUTTON_ID_SELECT, 500, select_long_click, NULL);
    window_single_click_subscribe(BUTTON_ID_UP, up_click);
    window_single_click_subscribe(BUTTON_ID_DOWN, down_click);
}

/* ---- Window lifecycle ----------------------------------------------- */
static void window_load(Window *window) {
    Layer *root = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(root);
    s_canvas = layer_create(bounds);
    layer_set_update_proc(s_canvas, canvas_update);
    layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
    layer_destroy(s_canvas);
}

static void init(void) {
    sc600_init(&s_game, (uint32_t)time(NULL) ^ 0x5C600u);
    set_status("SELECT = Spin");

    s_window = window_create();
    window_set_background_color(s_window, COL_BG);
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers){
        .load = window_load,
        .unload = window_unload,
    });
    window_stack_push(s_window, true);
}

static void deinit(void) {
    window_destroy(s_window);
}

int main(void) {
    init();
    app_event_loop();
    deinit();
}
