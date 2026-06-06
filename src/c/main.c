/*
 * main.c — Super Cherry / Super Lady fuer Pebble (emery + alle Plattformen).
 *
 * Layout angelehnt an Super Lady (Sonderspiele):
 *   - oben: Guthaben-Display + Wallet
 *   - oben-mitte: Pentagram (Sternausspielung) mit 5 Feldern
 *   - links: Cherry-Leiter (ACTION / 100x / 10x / 5x ...)
 *   - rechts: Mystery-Ausspielung (x0/x2/x3/x5)
 *   - mitte-unten: 3 Walzen + STOPP-Lauflicht
 *   - unten: Statuszeile + Tastenhinweise
 *
 * Steuerung (3 Tasten, kontextabhaengig):
 *   IDLE:      SELECT=Spin | UP=Muenze einwerfen (bzw. Wallet aufladen wenn low)
 *              DOWN=Einsatz + | (UP lang=Auszahlen)
 *   RUNLIGHT:  SELECT=Stoppen (bei STOP kassieren)
 *   GAMBLE:    UP=2x | DOWN=Mystery | SELECT=nehmen
 *   2X:        UP=linkes Feld | DOWN=rechtes Feld
 *   STAR:      SELECT=Pentagram drehen / Bonus spielen
 *   CHERRY-COLLECT: SELECT=Step / laeuft automatisch
 *
 * Persistenz via persistent_storage (Key 1).
 */
#include <pebble.h>
#include "sc_core.h"
#include "sc_symbols.h"

#define PERSIST_KEY_SAVE 1

static Window  *s_window;
static Layer   *s_canvas;
static SCGame   s_game;

static AppTimer *s_timer;
static int       s_anim_frames;
static bool      s_animating;
static char      s_status[64];
static int       s_winlost;  /* 0=aus, 1=WIN leuchtet, 2=LOST leuchtet */

/* Farben — Super-Lady metallic/grau Stil */
#ifdef PBL_COLOR
  #define BG        GColorBlack
  #define STEEL     GColorLightGray
  #define STEEL_D   GColorDarkGray
  #define PANEL     GColorDarkGray
  #define PANEL_D   GColorBlack
  #define FRAME     GColorLightGray
  #define LINE      GColorChromeYellow
  #define TXT       GColorWhite
  #define DISPLAY   GColorBlack
  #define DISPTXT   GColorBrightGreen
  #define WINCOL    GColorBrightGreen
  #define GOLD      GColorChromeYellow
  #define GOLD_D    GColorWindsorTan
  #define LIGHTON   GColorYellow
  #define MYSTCOL   GColorPictonBlue
  #define STOPCOL   GColorRed
  #define REELBG    GColorWhite
  #define REELBG_W  GColorPastelYellow
#else
  #define BG        GColorBlack
  #define STEEL     GColorWhite
  #define STEEL_D   GColorDarkGray
  #define PANEL     GColorBlack
  #define PANEL_D   GColorBlack
  #define FRAME     GColorWhite
  #define LINE      GColorWhite
  #define TXT       GColorWhite
  #define DISPLAY   GColorBlack
  #define DISPTXT   GColorWhite
  #define WINCOL    GColorWhite
  #define GOLD      GColorWhite
  #define GOLD_D    GColorWhite
  #define LIGHTON   GColorWhite
  #define MYSTCOL   GColorWhite
  #define STOPCOL   GColorWhite
  #define REELBG    GColorWhite
  #define REELBG_W  GColorWhite
#endif

static void set_status(const char *t){ snprintf(s_status,sizeof(s_status),"%s",t); }

static void save_game(void){
    uint8_t buf[64];
    int n = sc_serialize(&s_game, buf, sizeof(buf));
    if (n > 0) persist_write_data(PERSIST_KEY_SAVE, buf, n);
}
static void load_game(void){
    if (persist_exists(PERSIST_KEY_SAVE)) {
        uint8_t buf[64];
        int n = persist_read_data(PERSIST_KEY_SAVE, buf, sizeof(buf));
        if (n > 0) sc_deserialize(&s_game, buf, n);
    }
}

/* Metallic-Kasten mit Highlight-Kante (Super-Lady-Look). */
static void bevel_box(GContext *ctx, GRect r, GColor fill, GColor dark, GColor light, int radius){
    graphics_context_set_fill_color(ctx, fill);
    graphics_fill_rect(ctx, r, radius, GCornersAll);
    graphics_context_set_stroke_color(ctx, dark);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_round_rect(ctx, r, radius);
    /* obere + linke Highlight-Kante */
    graphics_context_set_stroke_color(ctx, light);
    graphics_draw_line(ctx, GPoint(r.origin.x+radius, r.origin.y+1),
                            GPoint(r.origin.x+r.size.w-radius, r.origin.y+1));
    graphics_draw_line(ctx, GPoint(r.origin.x+1, r.origin.y+radius),
                            GPoint(r.origin.x+1, r.origin.y+r.size.h-radius));
}

/* ===== Layout-Geometrie ===== */
/* Wir nutzen feste Bereiche, skaliert auf die Display-Hoehe. */
typedef struct { int reel_x0, reel_y0, cell, gap, grid_w, grid_h, stop_x, line_y; } ReelGeom;

static ReelGeom reel_geom(GRect b){
    ReelGeom g;
    g.cell = 26;
    if (b.size.w < 180) g.cell = 22;       /* schmalere Plattformen */
    g.gap = 3;
    g.grid_w = g.cell*3 + g.gap*2;
    g.grid_h = g.cell*3 + g.gap*2;
    int lw = 28;                            /* Leiter-Breite links */
    int rw = 28;                            /* Mystery-Breite rechts */
    int stopw = 26;                         /* breiter, damit STOP reinpasst */
    int zone_l = lw + 2;
    int zone_r = b.size.w - rw - 2;
    int total  = g.grid_w + 5 + stopw;      /* Walzen + Luecke + STOP */
    g.reel_x0  = zone_l + ((zone_r - zone_l) - total)/2;
    if (g.reel_x0 < zone_l) g.reel_x0 = zone_l;
    g.reel_y0  = b.size.h - g.grid_h - 28;
    g.stop_x   = g.reel_x0 + g.grid_w + 5;
    g.line_y   = g.reel_y0 + g.cell + g.gap + g.cell/2;
    return g;
}

/* ===== Einzelzeichnungen ===== */
static void draw_display(GContext *ctx, GRect b){
    GRect disp = GRect(4, 2, b.size.w-8, 19);
    bevel_box(ctx, disp, DISPLAY, STEEL_D, STEEL, 3);
    char cr[16]; snprintf(cr,sizeof(cr),"CR %d",(int)s_game.credits);
    char fr[20]; snprintf(fr,sizeof(fr),"FR %d.%02d",(int)(s_game.wallet/100),(int)(s_game.wallet%100));
    graphics_context_set_text_color(ctx, DISPTXT);
    graphics_draw_text(ctx, cr, fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(disp.origin.x+4, disp.origin.y, disp.size.w/2-4, 17),
        GTextOverflowModeFill, GTextAlignmentLeft, NULL);
    graphics_draw_text(ctx, fr, fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(disp.origin.x+disp.size.w/2, disp.origin.y, disp.size.w/2-6, 17),
        GTextOverflowModeFill, GTextAlignmentRight, NULL);
}

static void draw_pentagram(GContext *ctx, GRect b){
    int pcx = b.size.w/2, pcy = 46;
    GRect box = GRect(pcx-44, 24, 88, 48);
    bevel_box(ctx, box, PANEL, STEEL_D, STEEL, 4);
    int R = 17;
    GRect cell = GRect(pcx-R, pcy-R, 2*R, 2*R);
    sc_draw_symbol(ctx, SYM_STAR, cell);
    static const int16_t cosp[5] = {    0,  951,  588, -588, -951 };
    static const int16_t sinp[5] = {-1000, -309,  809,  809, -309 };
    for (int i = 0; i < 5; i++){
        int px = pcx + cosp[i]*(R*92/100)/1000;
        int py = pcy + sinp[i]*(R*92/100)/1000;
        bool active = (s_game.phase==PHASE_STAR && s_game.pentagram_pick==i);
        graphics_context_set_fill_color(ctx, active ? LIGHTON : GOLD_D);
        graphics_fill_circle(ctx, GPoint(px,py), active?5:3);
    }
    if (s_game.phase==PHASE_STAR){
        const char* nm[5]={"FRUIT","2x SH","4x SH","10x","COLL"};
        graphics_context_set_text_color(ctx, GOLD);
        graphics_draw_text(ctx, nm[s_game.pentagram_pick],
            fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(box.origin.x, box.origin.y+box.size.h-15, box.size.w, 15),
            GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
}

/* WIN / LOST Lichter unter dem Stern, ueber den Walzen — fuer Gamble 2x. */
static void draw_winlost(GContext *ctx, GRect b){
    int cx = b.size.w/2, y = 76, h = 16, bw = 40;
    /* aktiv nur direkt nach einem 2x-Ergebnis (an last_gamble_light gekoppelt) */
    bool win_on  = (s_winlost == 1);
    bool lost_on = (s_winlost == 2);
    /* WIN */
    GRect wr = GRect(cx-bw-3, y, bw, h);
    graphics_context_set_fill_color(ctx, win_on ? GColorGreen : PANEL_D);
    graphics_fill_rect(ctx, wr, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, STEEL_D);
    graphics_draw_round_rect(ctx, wr, 4);
    graphics_context_set_text_color(ctx, win_on ? GColorBlack : GColorDarkGray);
    graphics_draw_text(ctx, "WIN", fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(wr.origin.x, wr.origin.y-1, wr.size.w, h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    /* LOST */
    GRect lr = GRect(cx+3, y, bw, h);
    graphics_context_set_fill_color(ctx, lost_on ? STOPCOL : PANEL_D);
    graphics_fill_rect(ctx, lr, 4, GCornersAll);
    graphics_context_set_stroke_color(ctx, STEEL_D);
    graphics_draw_round_rect(ctx, lr, 4);
    graphics_context_set_text_color(ctx, lost_on ? GColorWhite : GColorDarkGray);
    graphics_draw_text(ctx, "LOST", fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(lr.origin.x, lr.origin.y-1, lr.size.w, h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

/* Cherry-Leiter links als metallic-Streifen mit Kirsch-Icon + Stufen. */
static void draw_cherry_ladder(GContext *ctx, GRect b){
    int lx=4, lw=24, ly=24;
    GRect strip = GRect(lx, ly, lw, b.size.h-28-ly);
    bevel_box(ctx, strip, PANEL_D, STEEL_D, STEEL, 4);
    GRect ci = GRect(lx, ly+3, lw, 16);
    sc_draw_symbol(ctx, SYM_CHERRY, ci);
    const char* labels[CC_LEVELS] = { "MAX","100","15","10","5" }; /* oben->unten */
    int n = CC_LEVELS;
    int slot_h = (strip.size.h - 22) / n;
    for (int i=0;i<n;i++){
        int level = CC_LEVELS - i;          /* 5..1 */
        int yy = ly + 20 + i*slot_h;
        bool active = (s_game.phase==PHASE_CHERRY_COLLECT && s_game.cc_level==level);
        if (active){
            graphics_context_set_fill_color(ctx, GOLD);
            graphics_fill_rect(ctx, GRect(lx+2, yy, lw-4, slot_h-2), 2, GCornersAll);
            graphics_context_set_text_color(ctx, GColorBlack);
        } else {
            graphics_context_set_text_color(ctx, GOLD);
        }
        graphics_draw_text(ctx, labels[i], fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(lx, yy+slot_h/2-8, lw, slot_h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
}

/* Mystery-Ausspielung rechts als metallic-Streifen (blau). */
static void draw_mystery_panel(GContext *ctx, GRect b){
    int lw=24, lx=b.size.w-lw-4, ly=24;
    GRect strip = GRect(lx, ly, lw, b.size.h-28-ly);
    bevel_box(ctx, strip, PANEL_D, STEEL_D, STEEL, 4);
    graphics_context_set_text_color(ctx, MYSTCOL);
    graphics_draw_text(ctx, "?", fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(lx, ly+1, lw, 18), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    const char* labels[4] = { "5","3","2","0" };
    int vals[4] = {5,3,2,0};
    int slot_h = (strip.size.h - 22) / 4;
    for (int i=0;i<4;i++){
        int yy = ly + 20 + i*slot_h;
        bool active = ((s_game.phase==PHASE_MYSTERY||s_game.phase==PHASE_GAMBLE)
                       && s_game.mystery_mult==vals[i]);
        if (active){
            graphics_context_set_fill_color(ctx, LIGHTON);
            graphics_fill_rect(ctx, GRect(lx+2, yy, lw-4, slot_h-2), 2, GCornersAll);
            graphics_context_set_text_color(ctx, GColorBlack);
        } else {
            graphics_context_set_text_color(ctx, MYSTCOL);
        }
        graphics_draw_text(ctx, labels[i], fonts_get_system_font(FONT_KEY_GOTHIC_14),
            GRect(lx, yy+slot_h/2-8, lw, slot_h), GTextOverflowModeFill, GTextAlignmentCenter, NULL);
    }
}

/* WINS-Tabelle als kompakte Linie unten ist zu eng; wir zeigen sie weg
 * und nutzen den Platz fuers Spiel. Stattdessen Top-Symbole im Status. */

static void draw_reels(GContext *ctx, GRect b){
    ReelGeom g = reel_geom(b);

    /* Walzen-Gehaeuse (metallic) */
    GRect housing = GRect(g.reel_x0-5, g.reel_y0-5, g.grid_w+10, g.grid_h+10);
    bevel_box(ctx, housing, GColorBlack, GColorBlack, STEEL, 5);

    /* Zellen */
    int rl_re=-1, rl_ro=-1; bool rl_stop=false;
    if (s_game.phase == PHASE_RUNLIGHT){
        sc_runlight_station(&s_game, &rl_re, &rl_ro);
        rl_stop = (rl_re == RL_STOP_REEL);
    }
    for (int r=0;r<SC_REELS;r++){
        int cx = g.reel_x0 + r*(g.cell+g.gap);
        for (int row=0;row<3;row++){
            int cy = g.reel_y0 + row*(g.cell+g.gap);
            GRect cell = GRect(cx, cy, g.cell, g.cell);
            graphics_context_set_fill_color(ctx, (row==1)?REELBG_W:REELBG);
            graphics_fill_rect(ctx, cell, 2, GCornersAll);
            graphics_context_set_stroke_color(ctx, STEEL_D);
            graphics_draw_rect(ctx, cell);
            sc_draw_symbol(ctx, s_game.window[r][row], cell);
            /* Lauflicht: aktive Zelle hell umranden */
            if (s_game.phase==PHASE_RUNLIGHT && !rl_stop && r==rl_re && row==rl_ro){
                graphics_context_set_stroke_color(ctx, LIGHTON);
                graphics_context_set_stroke_width(ctx, 3);
                graphics_draw_rect(ctx, GRect(cx-1, cy-1, g.cell+2, g.cell+2));
                graphics_context_set_stroke_width(ctx, 1);
            }
        }
    }

    /* Gewinnlinie (mittlere Reihe) -> fuehrt zum STOP-Feld */
    graphics_context_set_stroke_color(ctx, LINE);
    graphics_context_set_stroke_width(ctx, 1);
    graphics_draw_line(ctx, GPoint(g.reel_x0-3, g.line_y),
                            GPoint(g.reel_x0+g.grid_w+2, g.line_y));

    /* Eigenes STOP-Feld rechts der Walzen */
    GRect stop = GRect(g.stop_x, g.line_y-14, 26, 28);
    bool stop_hot = (s_game.phase==PHASE_RUNLIGHT && rl_stop);
    if (stop_hot){
        bevel_box(ctx, stop, STOPCOL, GColorBlack, GColorWhite, 3);
        graphics_context_set_text_color(ctx, GColorWhite);
    } else {
        bevel_box(ctx, stop, PANEL, STEEL_D, STEEL, 3);
        graphics_context_set_text_color(ctx, STOPCOL);
    }
    graphics_draw_text(ctx, "STOP", fonts_get_system_font(FONT_KEY_GOTHIC_14),
        GRect(stop.origin.x-3, stop.origin.y+5, stop.size.w+6, 18),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_status(GContext *ctx, GRect b){
    GRect bar = GRect(0, b.size.h-22, b.size.w, 22);
    graphics_context_set_fill_color(ctx, PANEL_D);
    graphics_fill_rect(ctx, bar, 0, GCornerNone);
    graphics_context_set_stroke_color(ctx, STEEL_D);
    graphics_draw_line(ctx, GPoint(0,b.size.h-22), GPoint(b.size.w,b.size.h-22));
    GColor sc = (s_game.last_win>0)?WINCOL:GOLD;
    graphics_context_set_text_color(ctx, sc);
    graphics_draw_text(ctx, s_status, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
        GRect(2, b.size.h-21, b.size.w-4, 20),
        GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

/* Gamble/2x werden ueber die immer sichtbare Mystery-Saeule + Statuszeile
 * kommuniziert, kein separates Vollbild (keine Overlays). */

/* ===== Haupt-Render ===== */
static void canvas_update(Layer *layer, GContext *ctx){
    GRect b = layer_get_bounds(layer);
    graphics_context_set_fill_color(ctx, BG);
    graphics_fill_rect(ctx, b, 0, GCornerNone);

    /* Layout ist IMMER gleich — kein Overlay, alles sichtbar. */
    draw_display(ctx, b);
    draw_pentagram(ctx, b);
    draw_winlost(ctx, b);
    draw_cherry_ladder(ctx, b);
    draw_mystery_panel(ctx, b);
    draw_reels(ctx, b);
    draw_status(ctx, b);
}

/* ===== Animationen ===== */
static void runlight_tick_cb(void *d);
static void start_runlight(void){
    s_animating = false; /* Lauflicht ist interaktiv, kein Block */
    s_timer = app_timer_register(sc_runlight_tick_ms(&s_game), runlight_tick_cb, NULL);
}
static void runlight_tick_cb(void *d){
    (void)d;
    if (s_game.phase != PHASE_RUNLIGHT) return;
    sc_runlight_tick(&s_game);
    layer_mark_dirty(s_canvas);
    s_timer = app_timer_register(sc_runlight_tick_ms(&s_game), runlight_tick_cb, NULL);
}

static void cc_tick_cb(void *d);
static void start_cc(void){
    s_animating = true;
    s_timer = app_timer_register(450, cc_tick_cb, NULL);
}
static void cc_tick_cb(void *d){
    (void)d;
    bool more = sc_cc_step(&s_game);
    char m[40];
    if (s_game.cc_level>0) snprintf(m,sizeof(m),"Cherry Collect Stufe %d",(int)s_game.cc_level);
    else snprintf(m,sizeof(m),"Cherry Collect laeuft...");
    set_status(m);
    layer_mark_dirty(s_canvas);
    if (more){ s_timer = app_timer_register(450, cc_tick_cb, NULL); }
    else {
        s_animating = false;
        char w[40]; snprintf(w,sizeof(w),"Collect: +%d",(int)s_game.cc_win);
        set_status(w); vibes_short_pulse();
        save_game();
        layer_mark_dirty(s_canvas);
    }
}

static void hold_tick_cb(void *d);
static void start_hold(void){
    s_animating = true; s_anim_frames = 8;
    s_timer = app_timer_register(60, hold_tick_cb, NULL);
}
static void after_features(void); /* fwd */
static void hold_tick_cb(void *d){
    (void)d;
    if (s_anim_frames > 0){
        for (int row=0;row<3;row++)
            s_game.window[2][row] = (Symbol)((s_game.window[2][row]+1)%SYM_COUNT);
        s_anim_frames--;
        layer_mark_dirty(s_canvas);
        s_timer = app_timer_register(60, hold_tick_cb, NULL);
        return;
    }
    sc_resolve_hold(&s_game);
    s_animating = false;
    after_features();
}

static void step_tick_cb(void *d);
static void start_step(void){
    s_animating = true;
    s_timer = app_timer_register(200, step_tick_cb, NULL);
}
static void step_tick_cb(void *d){
    (void)d;
    bool more = sc_step_advance(&s_game);
    layer_mark_dirty(s_canvas);
    if (more){ s_timer = app_timer_register(200, step_tick_cb, NULL); }
    else { s_animating = false; after_features(); }
}

/* Nach Hold/Step: Phase ist RUNLIGHT (sc_*_advance/resolve ruft runlight_start). */
static void after_features(void){
    if (s_game.phase == PHASE_RUNLIGHT){
        set_status("STOPP bei STOP druecken!");
        start_runlight();
    }
    layer_mark_dirty(s_canvas);
}

static void spin_tick_cb(void *d);
static void start_spin(void){
    s_animating = true; s_anim_frames = 12;
    s_timer = app_timer_register(45, spin_tick_cb, NULL);
}
static void spin_tick_cb(void *d){
    (void)d;
    if (s_anim_frames > 0){
        for (int r=0;r<SC_REELS;r++)
            for (int row=0;row<3;row++)
                s_game.window[r][row]=(Symbol)((s_game.window[r][row]+1+r)%SYM_COUNT);
        s_anim_frames--;
        layer_mark_dirty(s_canvas);
        s_timer = app_timer_register(45+(12-s_anim_frames)*5, spin_tick_cb, NULL);
        return;
    }
    sc_restore_result(&s_game);
    s_animating = false;
    sc_after_spin(&s_game);
    if (s_game.phase == PHASE_HOLD){ set_status("HOLD"); start_hold(); }
    else if (s_game.phase == PHASE_STEP){ set_status("STEP"); start_step(); }
    else after_features();
}

static void do_spin(void){
    if (s_animating) return;
    if (s_game.phase != PHASE_IDLE) return;
    s_winlost = 0;
    if (!sc_spin(&s_game)){ set_status("Wenig CR! UP=Muenze"); layer_mark_dirty(s_canvas); return; }
    sc_save_result(&s_game);
    set_status("...dreht...");
    start_spin();
}

/* Star-Feature: erst Pentagram drehen, dann Bonus spielen. */
static void star_resolve(void){
    sc_star_spin_pentagram(&s_game);
    const char* names[]={"Fruit Stop","2x Shuffle","4x Shuffle","10x","Cherry Collect"};
    char m[48]; snprintf(m,sizeof(m),"Stern: %s",names[s_game.star_game]);
    set_status(m);
    int32_t w = sc_star_play(&s_game);
    if (s_game.phase == PHASE_CHERRY_COLLECT){ start_cc(); }
    else {
        char r[40]; snprintf(r,sizeof(r),"Stern: +%d",(int)w);
        set_status(r); vibes_short_pulse(); save_game();
    }
    layer_mark_dirty(s_canvas);
}

/* ===== Buttons ===== */
static void select_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    switch (s_game.phase){
        case PHASE_IDLE:
            if (!s_animating) do_spin();
            break;
        case PHASE_RUNLIGHT: {
            bool was_stop = sc_runlight_at_stop(&s_game);
            sc_runlight_press(&s_game);
            if (was_stop){
                if (s_game.last_win>0){ char m[40]; snprintf(m,sizeof(m),"Win %d! UP2x DN? SEL",(int)s_game.last_win); set_status(m); vibes_short_pulse(); }
                else set_status("Trostbetrag");
                if (s_game.phase==PHASE_STAR) star_resolve();
                save_game();
            } else {
                /* daneben: Gewinn verfallen */
                set_status("Verpasst - leider nichts");
                vibes_double_pulse();
                save_game();
            }
            break;
        }
        case PHASE_GAMBLE:
            { int32_t w=s_game.last_win; sc_gamble_take(&s_game);
              char m[40]; snprintf(m,sizeof(m),"genommen: %d",(int)w); set_status(m);
              if (s_game.phase==PHASE_STAR) star_resolve(); else save_game(); }
            break;
        case PHASE_STAR:
            star_resolve();
            break;
        default: break;
    }
    layer_mark_dirty(s_canvas);
}

static void up_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    switch (s_game.phase){
        case PHASE_IDLE:
            if (sc_wallet_low(&s_game)){
                sc_wallet_topup(&s_game);
                set_status("Wallet aufgeladen +20.-");
            } else if (sc_insert_coin(&s_game)){
                set_status("Muenze eingeworfen +10 CR");
            } else {
                set_status("Wallet leer!");
            }
            save_game();
            break;
        case PHASE_GAMBLE:
            sc_gamble_start_2x(&s_game);
            set_status("2x: UP oder DOWN tippen");
            break;
        case PHASE_GAMBLE_2X: {
            bool won=sc_gamble_2x_reveal(&s_game,0);
            s_winlost = won ? 1 : 2;
            set_status(won?"GEWONNEN!":"verloren...");
            if(won)vibes_short_pulse(); else vibes_double_pulse();
            if(s_game.phase==PHASE_STAR) star_resolve(); else save_game();
            break;
        }
        default: break;
    }
    layer_mark_dirty(s_canvas);
}

static void up_long_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    if (s_game.phase==PHASE_IDLE && !s_animating){
        sc_cashout(&s_game);
        set_status("Ausgezahlt");
        save_game();
        layer_mark_dirty(s_canvas);
    }
}

static void down_click(ClickRecognizerRef rec, void *ctx){
    (void)rec;(void)ctx;
    switch (s_game.phase){
        case PHASE_IDLE: {
            if (s_animating) break;
            sc_cycle_bet(&s_game);
            char m[24]; snprintf(m,sizeof(m),"Einsatz %d",(int)s_game.bet); set_status(m);
            save_game();
            break;
        }
        case PHASE_GAMBLE:
            sc_gamble_start_mystery(&s_game);
            { int m=sc_mystery_reveal(&s_game);
              char t[40]; snprintf(t,sizeof(t),"Mystery x%d = %d",m,(int)s_game.last_win);
              set_status(t);
              if(m>0)vibes_short_pulse(); else vibes_double_pulse();
              if(s_game.phase==PHASE_STAR) star_resolve(); else save_game(); }
            break;
        case PHASE_GAMBLE_2X: {
            bool won=sc_gamble_2x_reveal(&s_game,1);
            s_winlost = won ? 1 : 2;
            set_status(won?"GEWONNEN!":"verloren...");
            if(won)vibes_short_pulse(); else vibes_double_pulse();
            if(s_game.phase==PHASE_STAR) star_resolve(); else save_game();
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

/* ===== Lifecycle ===== */
static void window_load(Window *w){
    Layer *root = window_get_root_layer(w);
    s_canvas = layer_create(layer_get_bounds(root));
    layer_set_update_proc(s_canvas, canvas_update);
    layer_add_child(root, s_canvas);
}
static void window_unload(Window *w){ (void)w; layer_destroy(s_canvas); }

static void init(void){
    sc_init(&s_game, (uint32_t)time(NULL) ^ 0x5C600u);
    load_game();
    s_game.phase = PHASE_IDLE;
    set_status("SELECT: Spin");
    s_window = window_create();
    window_set_background_color(s_window, BG);
    window_set_click_config_provider(s_window, click_config);
    window_set_window_handlers(s_window, (WindowHandlers){
        .load=window_load, .unload=window_unload });
    window_stack_push(s_window, true);
}
static void deinit(void){ save_game(); window_destroy(s_window); }

int main(void){ init(); app_event_loop(); deinit(); }
