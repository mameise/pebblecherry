/*
 * sc600_core.c — Super Cherry 600 Spiellogik (plattformunabhaengig)
 * Siehe sc600_core.h fuer die API-Beschreibung.
 */
#include "sc600_core.h"

/* --- RNG: xorshift32 (deterministisch, portabel) --------------------- */
static uint32_t rng_next(SC600Game *g) {
    uint32_t x = g->rng_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng_state = x ? x : 0x1234567u;
    return g->rng_state;
}

static int rng_range(SC600Game *g, int n) {
    return (int)(rng_next(g) % (uint32_t)n);
}

/* --- Walzenbaender ---------------------------------------------------
 * Jedes Band gewichtet Symbole. Kirsche und Stern bewusst seltener.
 * Reihenfolge variiert leicht pro Walze, damit Bilder nicht zu oft
 * dreifach auftreten (haelt RTP gefuehlt im klassischen Bereich).
 */
static const Symbol REEL_STRIP[SC_REELS][SC_REEL_LEN] = {
    { SYM_LEMON, SYM_ORANGE, SYM_CHERRY, SYM_PLUM, SYM_BELL, SYM_GRAPE,
      SYM_BAR,   SYM_LEMON,  SYM_MELON,  SYM_ORANGE, SYM_SEVEN, SYM_CHERRY,
      SYM_PLUM,  SYM_GRAPE,  SYM_STAR,   SYM_BELL },
    { SYM_ORANGE, SYM_LEMON, SYM_GRAPE, SYM_CHERRY, SYM_BELL, SYM_PLUM,
      SYM_MELON,  SYM_BAR,   SYM_ORANGE, SYM_LEMON, SYM_CHERRY, SYM_SEVEN,
      SYM_GRAPE,  SYM_PLUM,  SYM_BELL,  SYM_STAR },
    { SYM_PLUM,  SYM_ORANGE, SYM_LEMON, SYM_BELL, SYM_CHERRY, SYM_GRAPE,
      SYM_BAR,   SYM_MELON,  SYM_ORANGE, SYM_SEVEN, SYM_LEMON, SYM_PLUM,
      SYM_CHERRY, SYM_GRAPE, SYM_STAR,  SYM_BELL }
};

/* --- Auszahlungstabelle (3 gleiche auf der Linie) -------------------- */
int32_t sc600_payout_three(Symbol s) {
    switch (s) {
        case SYM_STAR:   return 600;  /* x600 — Maximalgewinn */
        case SYM_SEVEN:  return 150;
        case SYM_BAR:    return 80;
        case SYM_BELL:   return 40;
        case SYM_MELON:  return 25;
        case SYM_GRAPE:  return 20;
        case SYM_PLUM:   return 14;
        case SYM_ORANGE: return 10;
        case SYM_LEMON:  return 8;
        case SYM_CHERRY: return 30;   /* 3 Kirschen extra stark */
        default:         return 0;
    }
}

char sc600_symbol_glyph(Symbol s) {
    switch (s) {
        case SYM_CHERRY: return 'C';
        case SYM_LEMON:  return 'L';
        case SYM_ORANGE: return 'O';
        case SYM_PLUM:   return 'P';
        case SYM_GRAPE:  return 'G';
        case SYM_MELON:  return 'M';
        case SYM_BELL:   return 'B';
        case SYM_BAR:    return '=';
        case SYM_SEVEN:  return '7';
        case SYM_STAR:   return '*';
        default:         return '?';
    }
}

/* --- Hilfen ---------------------------------------------------------- */
static void fill_reel(SC600Game *g, int reel) {
    /* Zufaellige Bandposition; window = 3 aufeinanderfolgende Symbole. */
    int pos = rng_range(g, SC_REEL_LEN);
    for (int row = 0; row < 3; row++) {
        g->window[reel][row] = REEL_STRIP[reel][(pos + row) % SC_REEL_LEN];
    }
}

static int count_visible_cherries(const SC600Game *g) {
    int c = 0;
    for (int r = 0; r < SC_REELS; r++)
        for (int row = 0; row < 3; row++)
            if (g->window[r][row] == SYM_CHERRY) c++;
    return c;
}

/* --- API ------------------------------------------------------------- */
void sc600_init(SC600Game *g, uint32_t seed) {
    for (int r = 0; r < SC_REELS; r++) {
        g->hold[r] = false;
        for (int row = 0; row < 3; row++) g->window[r][row] = SYM_LEMON;
    }
    g->phase        = PHASE_IDLE;
    g->credits      = SC_START_CREDITS;
    g->bet          = SC_DEFAULT_BET;
    g->last_win     = 0;
    g->cherry_pot   = 0;
    g->step_armed   = false;
    g->cherry_step  = false;
    g->can_gamble   = false;
    g->gamble_stake = 0;
    g->gamble_step  = 0;
    g->last_gamble  = GAMBLE_NONE;
    g->rng_state    = seed ? seed : 0xC0FFEEu;
    g->spins_total  = 0;
}

void sc600_cycle_bet(SC600Game *g) {
    if (g->phase != PHASE_IDLE) return;
    g->bet++;
    if (g->bet > SC_MAX_BET) g->bet = SC_DEFAULT_BET;
}

bool sc600_spin(SC600Game *g) {
    if (g->phase != PHASE_IDLE && g->phase != PHASE_HOLD) return false;
    if (g->credits < g->bet) return false;

    /* Bei frischem Spin (nicht aus Hold) Einsatz abziehen. */
    if (g->phase == PHASE_IDLE) {
        g->credits -= g->bet;
    }

    for (int r = 0; r < SC_REELS; r++) {
        if (!g->hold[r]) fill_reel(g, r);
    }
    /* Holds nach dem Dreh zuruecksetzen. */
    for (int r = 0; r < SC_REELS; r++) g->hold[r] = false;

    g->phase = PHASE_SPIN;
    g->spins_total++;
    g->last_win = 0;
    return true;
}

void sc600_toggle_hold(SC600Game *g, int reel) {
    if (reel < 0 || reel >= SC_REELS) return;
    if (g->phase != PHASE_HOLD) return;
    g->hold[reel] = !g->hold[reel];
}

void sc600_do_step(SC600Game *g) {
    if (!g->step_armed) return;
    /* Nudge: schiebe jede nicht-gehaltene Walze um 1 Symbol weiter. */
    for (int r = 0; r < SC_REELS; r++) {
        if (g->hold[r]) continue;
        Symbol top = g->window[r][0];
        /* einfache Rotation als Step-Optik */
        g->window[r][0] = g->window[r][1];
        g->window[r][1] = g->window[r][2];
        g->window[r][2] = top;
    }
    /* Cherry Step: erscheint Kirsche auf der Linie -> Bonusmarkierung. */
    g->cherry_step = false;
    for (int r = 0; r < SC_REELS; r++) {
        if (g->window[r][1] == SYM_CHERRY) { g->cherry_step = true; break; }
    }
    g->step_armed = false;
    g->phase = PHASE_EVAL;
}

int32_t sc600_evaluate(SC600Game *g) {
    int32_t win = 0;

    Symbol a = g->window[0][1];
    Symbol b = g->window[1][1];
    Symbol c = g->window[2][1];

    /* 3 gleiche auf der Gewinnlinie */
    if (a == b && b == c) {
        win = sc600_payout_three(a) * g->bet;
    } else {
        /* Kirschen-Sonderregel: zahlen schon bei 1 oder 2 auf der Linie */
        int cherries_line = (a == SYM_CHERRY) + (b == SYM_CHERRY) + (c == SYM_CHERRY);
        if (cherries_line == 1) win = 2  * g->bet;
        else if (cherries_line == 2) win = 6 * g->bet;
    }

    /* Cherry Step Bonus addiert einen kleinen Zusatzgewinn. */
    if (g->cherry_step) {
        win += 5 * g->bet;
        g->cherry_step = false;
    }

    /* Cherry Pot: jede SICHTBARE Kirsche fuellt den Topf (auch ausserhalb Linie). */
    int vis = count_visible_cherries(g);
    g->cherry_pot += vis * SC_POT_PER_CHERRY;
    if (g->cherry_pot > SC_POT_THRESHOLD) g->cherry_pot = SC_POT_THRESHOLD;

    /* Step wird scharf, wenn genau 2 gleiche (ausser Kirsche) auf der Linie. */
    g->step_armed = false;
    if (!(a == b && b == c)) {
        if ((a == b && a != SYM_CHERRY) ||
            (b == c && b != SYM_CHERRY) ||
            (a == c && a != SYM_CHERRY)) {
            g->step_armed = true;
        }
    }

    g->last_win   = win;
    g->credits   += win;
    g->can_gamble = (win > 0);
    if (g->can_gamble) {
        g->gamble_stake = win;
        g->gamble_step  = 0;
        g->last_gamble  = GAMBLE_NONE;
    }
    g->phase = PHASE_EVAL;
    return win;
}

int32_t sc600_collect_pot(SC600Game *g) {
    if (g->cherry_pot < SC_POT_THRESHOLD) return 0;
    /* Pot-Wert skaliert mit Einsatz, gedeckelt bei x600-Logik. */
    int32_t payout = SC_POT_THRESHOLD * g->bet; /* spuerbarer, fairer Bonus */
    g->credits   += payout;
    g->cherry_pot = 0;
    g->phase      = PHASE_IDLE;
    return payout;
}

GambleResult sc600_gamble(SC600Game *g, int choice) {
    if (!g->can_gamble || g->gamble_stake <= 0) return GAMBLE_NONE;
    int roll = rng_range(g, 2); /* 0 oder 1 */
    if (roll == choice) {
        g->gamble_stake *= 2;
        g->gamble_step++;
        g->last_gamble = GAMBLE_WON;
        /* Deckel: maximale Risikoleiter, danach Zwangs-Collect. */
        if (g->gamble_stake >= SC_MAX_MULTI * g->bet) {
            g->gamble_stake = SC_MAX_MULTI * g->bet;
            g->can_gamble = false;
        }
        return GAMBLE_WON;
    } else {
        g->gamble_stake = 0;
        g->can_gamble   = false;
        g->last_gamble  = GAMBLE_LOST;
        g->last_win     = 0;
        g->phase        = PHASE_IDLE;
        return GAMBLE_LOST;
    }
}

void sc600_gamble_collect(SC600Game *g) {
    if (g->gamble_stake > 0) {
        /* Gewinn wurde bei evaluate bereits gutgeschrieben; hier nur die
         * Differenz aus dem Gamble nachbuchen. */
        g->credits += (g->gamble_stake - g->last_win);
        g->last_win = g->gamble_stake;
    }
    g->gamble_stake = 0;
    g->can_gamble   = false;
    g->phase        = PHASE_IDLE;
}

/* --- Animations-Backup ----------------------------------------------- */
static Symbol s_backup[SC_REELS][3];

void sc600_save_result(SC600Game *g) {
    for (int r = 0; r < SC_REELS; r++)
        for (int row = 0; row < 3; row++)
            s_backup[r][row] = g->window[r][row];
}

void sc600_restore_result(SC600Game *g) {
    for (int r = 0; r < SC_REELS; r++)
        for (int row = 0; row < 3; row++)
            g->window[r][row] = s_backup[r][row];
}
