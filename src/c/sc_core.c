/*
 * sc_core.c — Super Cherry Spiellogik (plattformunabhaengig)
 * Echte Mechanik. Siehe sc_core.h.
 */
#include "sc_core.h"

/* --- RNG: xorshift32 ------------------------------------------------- */
static uint32_t rng_next(SCGame *g) {
    uint32_t x = g->rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng_state = x ? x : 0x1234567u;
    return g->rng_state;
}
static int rng_range(SCGame *g, int n) {
    return (int)(rng_next(g) % (uint32_t)n);
}

/* --- Walzenbaender ---------------------------------------------------
 * Gewichtung ueber Haeufigkeit im Band. BAR/Stern selten, Fruechte oft.
 * Kirsche mittel-selten. Jede Walze leicht anders.
 */
static const Symbol REEL_STRIP[SC_REELS][SC_REEL_LEN] = {
    { SYM_GRAPE, SYM_LEMON, SYM_CHERRY, SYM_ORANGE, SYM_PEAR, SYM_PLUM,
      SYM_BELL,  SYM_LEMON, SYM_MELON,  SYM_ORANGE, SYM_STAR, SYM_GRAPE,
      SYM_BAR,   SYM_PLUM,  SYM_CHERRY, SYM_PEAR },
    { SYM_LEMON, SYM_ORANGE, SYM_PEAR, SYM_CHERRY, SYM_GRAPE, SYM_PLUM,
      SYM_MELON, SYM_LEMON,  SYM_BELL, SYM_ORANGE, SYM_GRAPE, SYM_STAR,
      SYM_PEAR,  SYM_PLUM,   SYM_BAR,  SYM_CHERRY },
    { SYM_ORANGE, SYM_PEAR, SYM_LEMON, SYM_GRAPE, SYM_CHERRY, SYM_PLUM,
      SYM_MELON,  SYM_BELL, SYM_ORANGE, SYM_LEMON, SYM_STAR,  SYM_PEAR,
      SYM_GRAPE,  SYM_BAR,  SYM_PLUM,  SYM_CHERRY }
};

/* --- Gewinntabelle (3 gleiche, Faktor × Einsatz) --------------------- */
int32_t sc_payout_three(Symbol s) {
    switch (s) {
        case SYM_BAR:    return 500;
        case SYM_STAR:   return 100;
        case SYM_BELL:   return 50;
        case SYM_CHERRY: return 20;
        case SYM_MELON:  return 10;
        case SYM_PLUM:   return 10;
        case SYM_ORANGE: return 5;
        case SYM_LEMON:  return 5;
        case SYM_PEAR:   return 2;
        case SYM_GRAPE:  return 2;
        default:         return 0;
    }
}

/* --- Hilfen ---------------------------------------------------------- */
static void place_reel(SCGame *g, int reel, int pos) {
    pos = ((pos % SC_REEL_LEN) + SC_REEL_LEN) % SC_REEL_LEN;
    g->reel_pos[reel] = pos;
    for (int row = 0; row < 3; row++)
        g->window[reel][row] = REEL_STRIP[reel][(pos + row) % SC_REEL_LEN];
}

static void spin_reel(SCGame *g, int reel) {
    place_reel(g, reel, rng_range(g, SC_REEL_LEN));
}

static int count_visible(const SCGame *g, Symbol s) {
    int c = 0;
    for (int r = 0; r < SC_REELS; r++)
        for (int row = 0; row < 3; row++)
            if (g->window[r][row] == s) c++;
    return c;
}

/* --- Init / Bet ------------------------------------------------------ */
void sc_init(SCGame *g, uint32_t seed) {
    g->rng_state = seed ? seed : 0xC0FFEEu;
    for (int r = 0; r < SC_REELS; r++) place_reel(g, r, r * 3);
    g->phase           = PHASE_IDLE;
    g->feature         = FEATURE_NONE;
    g->step_nudges_left= 0;
    g->cherry_step     = false;
    g->credits         = SC_START_CREDITS;
    g->bet             = SC_DEFAULT_BET;
    g->last_win        = 0;
    g->base_win        = 0;
    g->can_gamble      = false;
    g->gamble_secured  = 0;
    g->gamble_round    = 0;
    g->mystery_mult    = 0;
    g->star_pending    = false;
    g->star_game       = STAR_FRUIT_STOP;
    g->star_win        = 0;
    g->spins_total     = 0;
}

void sc_cycle_bet(SCGame *g) {
    if (g->phase != PHASE_IDLE) return;
    g->bet++;
    if (g->bet > SC_MAX_BET) g->bet = SC_DEFAULT_BET;
}

/* --- Spin ------------------------------------------------------------ */
bool sc_spin(SCGame *g) {
    if (g->phase != PHASE_IDLE) return false;
    if (g->credits < g->bet) return false;
    g->credits     -= g->bet;
    g->last_win     = 0;
    g->base_win     = 0;
    g->can_gamble   = false;
    g->gamble_secured = 0;
    g->gamble_round = 0;
    g->cherry_step  = false;
    g->feature      = FEATURE_NONE;
    g->star_pending = false;
    g->star_win     = 0;
    for (int r = 0; r < SC_REELS; r++) spin_reel(g, r);
    g->phase = PHASE_SPIN;
    g->spins_total++;
    return true;
}

/* Prueft die Linie auf "fast"-Treffer (2 gleich) und entscheidet
 * zufaellig Hold vs Step. Liefert das doppelte Symbol & welche Walze
 * abweicht. Gibt -1 zurueck wenn kein 2er-Paar. */
static int find_pair(const SCGame *g, Symbol *pair_sym, int *odd_reel) {
    Symbol a = g->window[0][1], b = g->window[1][1], c = g->window[2][1];
    if (a == b && b == c) return -1;             /* schon Tripel */
    if (a == b) { *pair_sym = a; *odd_reel = 2; return 1; }
    if (b == c) { *pair_sym = b; *odd_reel = 0; return 1; }
    if (a == c) { *pair_sym = a; *odd_reel = 1; return 1; }
    return -1;
}

void sc_after_spin(SCGame *g) {
    Symbol a = g->window[0][1], b = g->window[1][1], c = g->window[2][1];

    /* 3 Sterne sichtbar (irgendwo) -> Star Feature vormerken. */
    if (count_visible(g, SYM_STAR) >= 3) {
        g->star_pending = true;
    }

    /* Direkter Tripel-Treffer (inkl. 3 Sterne auf Linie). */
    if (a == b && b == c) {
        sc_evaluate(g);
        return;
    }

    /* Kirschen-Sonderregel zaehlt auch ohne Paar als Gewinn. */
    int cherries_line = (a==SYM_CHERRY)+(b==SYM_CHERRY)+(c==SYM_CHERRY);

    /* Genau 2 gleiche auf der Linie -> zufaellig Hold oder Step. */
    Symbol pair; int odd;
    if (find_pair(g, &pair, &odd) == 1) {
        if (rng_range(g, 2) == 0) {
            g->feature = FEATURE_HOLD;
            g->phase   = PHASE_HOLD;       /* Frontend animiert Re-Spin */
        } else {
            g->feature = FEATURE_STEP;
            g->step_nudges_left = SC_STEP_MAX_NUDGES;
            g->cherry_step = false;
            g->phase = PHASE_STEP;          /* Frontend ruft step_advance */
        }
        return;
    }

    /* Kein Paar, evtl. Kirschengewinn (1/2 Kirschen). */
    if (cherries_line >= 1) {
        sc_evaluate(g);
        return;
    }

    /* Kein Gewinn, aber Star evtl. vorgemerkt. */
    sc_evaluate(g);
}

/* Hold: die abweichende Walze nochmal drehen, dann auswerten. */
void sc_resolve_hold(SCGame *g) {
    Symbol pair; int odd;
    if (find_pair(g, &pair, &odd) == 1) {
        spin_reel(g, odd);
    }
    g->feature = FEATURE_NONE;
    sc_evaluate(g);
}

/* Step: abweichende Walze um 1 Symbol weiterruecken (Nudge).
 * Trifft sie das Paar-Symbol -> Treffer. Erscheint Kirsche und die
 * anderen sind weder BAR noch Glocke -> Cherry Step (3 Kirschen). */
bool sc_step_advance(SCGame *g) {
    Symbol pair; int odd;
    if (find_pair(g, &pair, &odd) != 1) {
        g->feature = FEATURE_NONE;
        sc_evaluate(g);
        return false;
    }

    /* Walze einen Schritt weiter. */
    place_reel(g, odd, g->reel_pos[odd] + 1);
    g->step_nudges_left--;

    Symbol now = g->window[odd][1];

    /* Cherry Step: Kirsche taucht auf, Paar ist nicht BAR/Glocke. */
    if (now == SYM_CHERRY && pair != SYM_BAR && pair != SYM_BELL) {
        /* Walzen formen 3 Kirschen. */
        for (int r = 0; r < SC_REELS; r++) g->window[r][1] = SYM_CHERRY;
        g->cherry_step = true;
        g->feature = FEATURE_NONE;
        sc_evaluate(g);
        return false;
    }

    /* Treffer: drittes Symbol passt zum Paar. */
    if (now == pair) {
        g->feature = FEATURE_NONE;
        sc_evaluate(g);
        return false;
    }

    /* Keine Schritte mehr -> Step endet ohne Treffer. */
    if (g->step_nudges_left <= 0) {
        g->feature = FEATURE_NONE;
        sc_evaluate(g);
        return false;
    }
    return true; /* weiter rucken */
}

/* --- Auswertung ------------------------------------------------------ */
int32_t sc_evaluate(SCGame *g) {
    Symbol a = g->window[0][1], b = g->window[1][1], c = g->window[2][1];
    int32_t win = 0;

    if (g->cherry_step) {
        /* Cherry Step garantiert 3 Kirschen = x20. */
        win = sc_payout_three(SYM_CHERRY) * g->bet;
    } else if (a == b && b == c) {
        win = sc_payout_three(a) * g->bet;
        /* 3 Sterne auf der Linie -> zusaetzlich x100 Sofortgewinn
         * (sc_payout_three(STAR) ist bereits 100, das deckt das ab). */
    } else {
        /* Kirschen-Sonderregel. */
        int left  = (a == SYM_CHERRY);
        int mid   = (b == SYM_CHERRY);
        int right = (c == SYM_CHERRY);
        int n = left + mid + right;
        if (n >= 2)      win = 4 * g->bet;   /* 2 Kirschen */
        else if (left || right) win = 2 * g->bet; /* 1 Kirsche aussen */
        /* 1 Kirsche nur in der Mitte zahlt nicht (Original-Regel). */
    }

    g->base_win   = win;
    g->last_win   = win;
    g->credits   += win;
    g->can_gamble = (win > 0);
    g->gamble_round = 0;
    g->gamble_secured = 0;

    if (win > 0) g->phase = PHASE_GAMBLE;     /* Gamble-Wahl anbieten */
    else if (g->star_pending) g->phase = PHASE_STAR;
    else g->phase = PHASE_IDLE;
    return win;
}

/* --- Gamble ---------------------------------------------------------- */
void sc_gamble_pick(SCGame *g, GamblePick pick) {
    if (!g->can_gamble) {
        if (g->star_pending) g->phase = PHASE_STAR; else g->phase = PHASE_IDLE;
        return;
    }
    switch (pick) {
        case GAMBLE_PICK_TAKE:
            g->can_gamble = false;
            if (g->star_pending) g->phase = PHASE_STAR;
            else g->phase = PHASE_IDLE;
            break;
        case GAMBLE_PICK_2X:
            g->phase = PHASE_GAMBLE_2X;
            break;
        case GAMBLE_PICK_MYSTERY:
            g->phase = PHASE_MYSTERY;
            break;
        case GAMBLE_PICK_SPLIT:
            /* Haelfte sichern, andere Haelfte bei 2x riskieren. */
            g->gamble_secured += g->last_win / 2;
            g->last_win       -= g->last_win / 2;
            g->phase = PHASE_GAMBLE_2X;
            break;
        default:
            break;
    }
}

static int32_t cap_to_version(SCGame *g, int32_t amount) {
    int32_t cap = SC_VERSION_MAX * g->bet;
    return amount > cap ? cap : amount;
}

bool sc_gamble_2x_reveal(SCGame *g, int choice) {
    g->gamble_round++;
    int roll = rng_range(g, 2);
    /* last_win liegt "auf dem Tisch" und wurde bei evaluate bereits
     * gutgeschrieben. Wir nehmen ihn vom Konto, der Ausgang entscheidet. */
    g->credits -= g->last_win;

    if (roll == choice) {
        /* Gewonnen: Betrag verdoppelt, bleibt auf dem Tisch. */
        g->last_win = cap_to_version(g, g->last_win * 2);

        bool forced = (g->gamble_round >= SC_GAMBLE_MAX_ROUNDS ||
                       g->last_win >= SC_VERSION_MAX * g->bet);
        if (forced) {
            /* Zwangsauszahlung: Tisch + gesicherter Split-Teil aufs Konto. */
            g->credits   += g->last_win + g->gamble_secured;
            g->last_win  += g->gamble_secured;
            g->gamble_secured = 0;
            g->can_gamble = false;
            g->phase = g->star_pending ? PHASE_STAR : PHASE_IDLE;
        } else {
            /* Tisch zurueck aufs Konto, damit Guthaben den Stand zeigt;
             * can_gamble bleibt, naechste Wahl folgt im Gamble-Menue. */
            g->credits += g->last_win;
            g->phase = PHASE_GAMBLE;
        }
        return true;
    } else {
        /* Verloren: riskierter Betrag weg. Gesicherter Split-Teil bleibt. */
        g->last_win = 0;
        if (g->gamble_secured > 0) {
            g->credits  += g->gamble_secured;
            g->last_win  = g->gamble_secured;
            g->gamble_secured = 0;
        }
        g->can_gamble = false;
        g->phase = g->star_pending ? PHASE_STAR : PHASE_IDLE;
        return false;
    }
}

int32_t sc_mystery_reveal(SCGame *g) {
    static const int mult[] = { 0, 2, 3, 5 };
    int m = mult[rng_range(g, 4)];
    g->mystery_mult = m;
    g->credits -= g->last_win;          /* riskierten Betrag vom Konto */
    g->last_win = cap_to_version(g, g->last_win * m);
    g->credits += g->last_win + g->gamble_secured;
    g->last_win += g->gamble_secured;
    g->gamble_secured = 0;
    g->can_gamble = false;
    g->phase = (g->star_pending) ? PHASE_STAR : PHASE_IDLE;
    return m;
}

/* --- Star Feature ---------------------------------------------------- */
int32_t sc_play_star(SCGame *g) {
    g->star_game = (StarGame)rng_range(g, 5);
    int32_t w = 0;
    switch (g->star_game) {
        case STAR_FRUIT_STOP:
            /* zufaellige Frucht bildet Gewinnlinie (Fruchtwerte x2..x10) */
            { Symbol fr = (Symbol)(SYM_GRAPE + rng_range(g, SYM_CHERRY - SYM_GRAPE));
              w = sc_payout_three(fr) * g->bet; }
            break;
        case STAR_SHUFFLE_2X:
            w = (5 + rng_range(g, 10)) * g->bet;  /* garantierter kleiner Gewinn */
            break;
        case STAR_SHUFFLE_4X:
            w = (10 + rng_range(g, 20)) * g->bet;
            break;
        case STAR_10X:
            w = 10 * g->bet;
            break;
        case STAR_CHERRY_COLLECT:
            /* Kirschen sammeln bis Versions-Maximum, hier gezogen. */
            w = (50 + rng_range(g, SC_VERSION_MAX - 50 + 1)) * g->bet;
            w = cap_to_version(g, w);
            break;
    }
    g->star_win   = w;
    g->credits   += w;
    g->star_pending = false;
    g->phase      = PHASE_IDLE;
    return w;
}

/* --- Animations-Backup ----------------------------------------------- */
static Symbol s_backup[SC_REELS][3];
static int32_t s_backup_pos[SC_REELS];

void sc_save_result(SCGame *g) {
    for (int r = 0; r < SC_REELS; r++) {
        s_backup_pos[r] = g->reel_pos[r];
        for (int row = 0; row < 3; row++) s_backup[r][row] = g->window[r][row];
    }
}
void sc_restore_result(SCGame *g) {
    for (int r = 0; r < SC_REELS; r++) {
        g->reel_pos[r] = s_backup_pos[r];
        for (int row = 0; row < 3; row++) g->window[r][row] = s_backup[r][row];
    }
}
