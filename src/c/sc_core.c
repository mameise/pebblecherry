/*
 * sc_core.c — Super Cherry / Super Lady Spiellogik.
 */
#include "sc_core.h"
#include <string.h>

/* --- RNG ------------------------------------------------------------- */
static uint32_t rng_next(SCGame *g) {
    uint32_t x = g->rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    g->rng_state = x ? x : 0x1234567u;
    return g->rng_state;
}
static int rng_range(SCGame *g, int n) { return (int)(rng_next(g) % (uint32_t)n); }

/* --- Walzenbaender --------------------------------------------------- */
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

/* --- Gewinntabelle --------------------------------------------------- */
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

/* --- STOPP-Lauflicht Pfad (10 Stationen, nahtloses Pendel) ----------- *
 * Position als (reel, row): reel 0..2, row 0=oben 1=mitte 2=unten.
 * STOP ist Sonderstation (reel=3). Das Licht pendelt nahtlos:
 * STOP -> R-mitte -> M-mitte -> L-mitte -> L-oben -> L-mitte -> L-unten ->
 * L-mitte -> M-mitte -> R-mitte -> (zurueck zu STOP). Jeder Schritt ist ein
 * echter Nachbarschritt, der Schleifen-Uebergang (R-mitte -> STOP) ebenso.
 */
static const RLStation RL_PATH[RL_PATH_LEN] = {
    {RL_STOP_REEL,1}, {2,1}, {1,1}, {0,1}, {0,0},
    {0,1}, {0,2}, {0,1}, {1,1}, {2,1}
};

/* --- Hilfen ---------------------------------------------------------- */
static void place_reel(SCGame *g, int reel, int pos) {
    pos = ((pos % SC_REEL_LEN) + SC_REEL_LEN) % SC_REEL_LEN;
    g->reel_pos[reel] = pos;
    for (int row = 0; row < 3; row++)
        g->window[reel][row] = REEL_STRIP[reel][(pos + row) % SC_REEL_LEN];
}
static void spin_reel(SCGame *g, int reel) { place_reel(g, reel, rng_range(g, SC_REEL_LEN)); }

static int count_visible(const SCGame *g, Symbol s) {
    int c = 0;
    for (int r = 0; r < SC_REELS; r++)
        for (int row = 0; row < 3; row++)
            if (g->window[r][row] == s) c++;
    return c;
}

static int find_pair(const SCGame *g, Symbol *pair_sym, int *odd_reel) {
    Symbol a = g->window[0][1], b = g->window[1][1], c = g->window[2][1];
    if (a == b && b == c) return -1;
    if (a == b) { *pair_sym = a; *odd_reel = 2; return 1; }
    if (b == c) { *pair_sym = b; *odd_reel = 0; return 1; }
    if (a == c) { *pair_sym = a; *odd_reel = 1; return 1; }
    return -1;
}

/* --- Init ------------------------------------------------------------ */
void sc_init(SCGame *g, uint32_t seed) {
    memset(g, 0, sizeof(*g));
    g->rng_state = seed ? seed : 0xC0FFEEu;
    for (int r = 0; r < SC_REELS; r++) place_reel(g, r, r * 3);
    g->phase   = PHASE_IDLE;
    g->credits = 50;            /* Start-Muenzspeicher */
    g->wallet  = SC_WALLET_START;
    g->bet     = SC_DEFAULT_BET;
}

void sc_cycle_bet(SCGame *g) {
    if (g->phase != PHASE_IDLE) return;
    g->bet++;
    if (g->bet > SC_MAX_BET) g->bet = SC_DEFAULT_BET;
}

/* --- Wallet ---------------------------------------------------------- */
bool sc_wallet_low(const SCGame *g) { return g->wallet < SC_WALLET_LOW; }

void sc_wallet_topup(SCGame *g) {
    if (sc_wallet_low(g)) g->wallet += SC_WALLET_TOPUP;
}

bool sc_insert_coin(SCGame *g) {
    if (g->wallet < SC_INSERT_COST) return false;
    g->wallet  -= SC_INSERT_COST;
    g->credits += SC_INSERT_CREDITS;
    return true;
}

void sc_cashout(SCGame *g) {
    if (g->phase != PHASE_IDLE) return;
    g->wallet  += g->credits * SC_COIN_VALUE;
    g->credits  = 0;
}

/* --- Spin ------------------------------------------------------------ */
bool sc_spin(SCGame *g) {
    if (g->phase != PHASE_IDLE) return false;
    if (g->credits < g->bet) return false;
    g->credits     -= g->bet;
    g->pending_win  = 0;
    g->last_win     = 0;
    g->can_gamble   = false;
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

int32_t sc_evaluate_line(SCGame *g) {
    Symbol a = g->window[0][1], b = g->window[1][1], c = g->window[2][1];
    int32_t win = 0;
    if (g->cherry_step) {
        win = sc_payout_three(SYM_CHERRY) * g->bet;
    } else if (a == b && b == c) {
        win = sc_payout_three(a) * g->bet;
    } else {
        int left = (a == SYM_CHERRY), mid = (b == SYM_CHERRY), right = (c == SYM_CHERRY);
        int n = left + mid + right;
        if (n >= 2) win = 4 * g->bet;
        else if (left || right) win = 2 * g->bet;
    }
    return win;
}

void sc_after_spin(SCGame *g) {
    /* Cherry Pot fuellen (alle sichtbaren Kirschen). */
    g->cherry_pot += count_visible(g, SYM_CHERRY);
    if (g->cherry_pot > SC_POT_THRESHOLD) g->cherry_pot = SC_POT_THRESHOLD;

    /* 3 Sterne sichtbar -> Star Feature vormerken. */
    if (count_visible(g, SYM_STAR) >= 3) g->star_pending = true;

    Symbol a = g->window[0][1], b = g->window[1][1], c = g->window[2][1];

    /* Tripel direkt? */
    if (a == b && b == c) {
        g->pending_win = sc_evaluate_line(g);
        sc_runlight_start(g);
        return;
    }
    /* genau 2 gleich -> zufaellig Hold/Step */
    Symbol pair; int odd;
    if (find_pair(g, &pair, &odd) == 1) {
        if (rng_range(g, 2) == 0) {
            g->feature = FEATURE_HOLD;
            g->phase   = PHASE_HOLD;
        } else {
            g->feature = FEATURE_STEP;
            g->step_nudges_left = SC_STEP_MAX_NUDGES;
            g->phase = PHASE_STEP;
        }
        return;
    }
    /* sonst: Liniengewinn (evtl. Kirschen) -> Lauflicht */
    g->pending_win = sc_evaluate_line(g);
    sc_runlight_start(g);
}

void sc_resolve_hold(SCGame *g) {
    Symbol pair; int odd;
    if (find_pair(g, &pair, &odd) == 1) spin_reel(g, odd);
    g->feature = FEATURE_NONE;
    if (count_visible(g, SYM_STAR) >= 3) g->star_pending = true;
    g->pending_win = sc_evaluate_line(g);
    sc_runlight_start(g);
}

bool sc_step_advance(SCGame *g) {
    Symbol pair; int odd;
    if (find_pair(g, &pair, &odd) != 1) {
        g->feature = FEATURE_NONE;
        g->pending_win = sc_evaluate_line(g);
        sc_runlight_start(g);
        return false;
    }
    place_reel(g, odd, g->reel_pos[odd] + 1);
    g->step_nudges_left--;
    Symbol now = g->window[odd][1];

    if (now == SYM_CHERRY && pair != SYM_BAR && pair != SYM_BELL) {
        for (int r = 0; r < SC_REELS; r++) g->window[r][1] = SYM_CHERRY;
        g->cherry_step = true;
        g->feature = FEATURE_NONE;
        g->pending_win = sc_evaluate_line(g);
        sc_runlight_start(g);
        return false;
    }
    if (now == pair || g->step_nudges_left <= 0) {
        g->feature = FEATURE_NONE;
        g->pending_win = sc_evaluate_line(g);
        sc_runlight_start(g);
        return false;
    }
    return true;
}

/* --- STOPP-Lauflicht ------------------------------------------------- */
void sc_runlight_start(SCGame *g) {
    g->rl_index    = 4;   /* L-oben: weit weg von STOP, Licht laeuft erst hin */
    g->rl_ticks    = 0;
    g->rl_won_spin = (g->pending_win > 0);
    g->phase       = PHASE_RUNLIGHT;
}

void sc_runlight_tick(SCGame *g) {
    g->rl_index = (g->rl_index + 1) % RL_PATH_LEN;
    g->rl_ticks++;
}

/* Tickdauer waechst mit der Zeit (startet schnell ~90ms, wird langsamer
 * bis ~360ms), gibt dem Spieler die Chance zu treffen. */
int32_t sc_runlight_tick_ms(const SCGame *g) {
    int32_t ms = 90 + g->rl_ticks * 6;
    if (ms > 360) ms = 360;
    return ms;
}

bool sc_runlight_at_stop(const SCGame *g) {
    return RL_PATH[g->rl_index].reel == RL_STOP_REEL;
}

void sc_runlight_station(const SCGame *g, int *reel, int *row) {
    *reel = RL_PATH[g->rl_index].reel;
    *row  = RL_PATH[g->rl_index].row;
}

void sc_runlight_press(SCGame *g) {
    if (g->phase != PHASE_RUNLIGHT) return;

    if (!sc_runlight_at_stop(g)) {
        /* Daneben gedrueckt: Lauflicht endet, Gewinn verfaellt komplett.
         * Auch ein anstehendes Star-Feature ist damit verloren. */
        g->pending_win  = 0;
        g->last_win     = 0;
        g->can_gamble   = false;
        g->star_pending = false;
        g->phase        = PHASE_IDLE;
        return;
    }

    /* Auf STOP getroffen. */
    if (g->rl_won_spin) {
        /* voller Gewinn: jetzt kassierbar -> Gamble anbieten */
        g->last_win   = g->pending_win;
        g->credits   += g->pending_win;
        g->can_gamble = (g->pending_win > 0);
        g->gamble_round = 0;
        if (g->can_gamble) {
            g->phase = PHASE_GAMBLE;
        } else if (g->star_pending) {
            g->phase = PHASE_STAR;
        } else {
            g->phase = PHASE_IDLE;
        }
    } else {
        /* kein Gewinn: 10% des Einsatzes zurueck (Trost) */
        int32_t trost = g->bet / 10;
        if (trost < 1 && g->bet >= 1) trost = 0; /* bei kleinem Einsatz evtl. 0 */
        g->last_win = trost;
        g->credits += trost;
        if (g->star_pending) g->phase = PHASE_STAR;
        else g->phase = PHASE_IDLE;
    }
}

/* --- Gamble (mehrfach hintereinander) -------------------------------- */
static int32_t cap_to_version(SCGame *g, int32_t amount) {
    int32_t cap = SC_VERSION_MAX * g->bet;
    return amount > cap ? cap : amount;
}

void sc_gamble_take(SCGame *g) {
    g->can_gamble = false;
    if (g->star_pending) g->phase = PHASE_STAR;
    else g->phase = PHASE_IDLE;
}

void sc_gamble_start_2x(SCGame *g) {
    if (g->can_gamble && g->last_win > 0) g->phase = PHASE_GAMBLE_2X;
}
void sc_gamble_start_mystery(SCGame *g) {
    if (g->can_gamble && g->last_win > 0) g->phase = PHASE_MYSTERY;
}

bool sc_gamble_2x_reveal(SCGame *g, int choice) {
    g->gamble_round++;
    int roll = rng_range(g, 2);
    g->credits -= g->last_win;   /* riskierten Betrag vom Konto nehmen */
    if (roll == choice) {
        g->last_win = cap_to_version(g, g->last_win * 2);
        g->credits += g->last_win;
        bool capped = (g->last_win >= SC_VERSION_MAX * g->bet);
        if (capped) { g->can_gamble = false; g->phase = g->star_pending ? PHASE_STAR : PHASE_IDLE; }
        else        { g->phase = PHASE_GAMBLE; } /* erneut waehlbar */
        return true;
    } else {
        g->last_win   = 0;
        g->can_gamble = false;
        g->phase = g->star_pending ? PHASE_STAR : PHASE_IDLE;
        return false;
    }
}

int32_t sc_mystery_reveal(SCGame *g) {
    static const int mult[] = { 0, 2, 3, 5 };
    int m = mult[rng_range(g, 4)];
    g->mystery_mult = m;
    g->credits -= g->last_win;
    g->last_win = cap_to_version(g, g->last_win * m);
    g->credits += g->last_win;
    if (m == 0 || g->last_win >= SC_VERSION_MAX * g->bet) {
        g->can_gamble = false;
        g->phase = g->star_pending ? PHASE_STAR : PHASE_IDLE;
    } else {
        g->phase = PHASE_GAMBLE; /* erneut waehlbar */
    }
    return m;
}

/* --- Star / Pentagram ------------------------------------------------ */
void sc_star_spin_pentagram(SCGame *g) {
    g->star_game = (StarGame)rng_range(g, 5);
    g->pentagram_pick = (int)g->star_game;
    g->phase = PHASE_STAR;
}

int32_t sc_star_play(SCGame *g) {
    int32_t w = 0;
    switch (g->star_game) {
        case STAR_FRUIT_STOP: {
            Symbol fr = (Symbol)(SYM_GRAPE + rng_range(g, SYM_CHERRY - SYM_GRAPE));
            w = sc_payout_three(fr) * g->bet;
            break;
        }
        case STAR_SHUFFLE_2X: w = (5 + rng_range(g, 10)) * g->bet; break;
        case STAR_SHUFFLE_4X: w = (10 + rng_range(g, 20)) * g->bet; break;
        case STAR_10X:        w = 10 * g->bet; break;
        case STAR_CHERRY_COLLECT:
            /* In die Cherry-Collect-Leiter wechseln statt Sofortbetrag. */
            sc_cc_start(g);
            return 0;
    }
    g->star_win   = w;
    g->credits   += w;
    g->last_win   = w;
    g->star_pending = false;
    g->phase = PHASE_IDLE;
    return w;
}

/* --- Cherry Collect Leiter ------------------------------------------- */
static int32_t cc_level_mult(int level) {
    /* level 1..5 -> x5, x10, x15, x100, xMAX */
    switch (level) {
        case 1: return 5;
        case 2: return 10;
        case 3: return 15;
        case 4: return 100;
        case 5: return SC_VERSION_MAX;
        default: return 0;
    }
}

void sc_cc_start(SCGame *g) {
    static const int step_choices[] = { 7, 10, 15 };
    g->cc_steps_left = step_choices[rng_range(g, 3)];
    g->cc_level = 0;
    g->cc_win   = 0;
    g->star_pending = false;
    g->phase = PHASE_CHERRY_COLLECT;
}

bool sc_cc_step(SCGame *g) {
    if (g->cc_steps_left <= 0 || g->cc_level >= CC_LEVELS) {
        /* fertig -> erreichte Stufe auszahlen */
        g->cc_win = cc_level_mult(g->cc_level) * g->bet;
        g->credits += g->cc_win;
        g->last_win = g->cc_win;
        g->phase = PHASE_IDLE;
        return false;
    }
    /* alle Walzen einen Schritt weiter */
    for (int r = 0; r < SC_REELS; r++) place_reel(g, r, g->reel_pos[r] + 1);
    g->cc_steps_left--;
    /* 3 Kirschen auf der Linie? -> Stufe hoch */
    if (g->window[0][1] == SYM_CHERRY && g->window[1][1] == SYM_CHERRY &&
        g->window[2][1] == SYM_CHERRY) {
        g->cc_level++;
        if (g->cc_level >= CC_LEVELS) {
            g->cc_win = cc_level_mult(CC_LEVELS) * g->bet;
            g->credits += g->cc_win;
            g->last_win = g->cc_win;
            g->phase = PHASE_IDLE;
            return false;
        }
    }
    if (g->cc_steps_left <= 0) {
        g->cc_win = cc_level_mult(g->cc_level) * g->bet;
        g->credits += g->cc_win;
        g->last_win = g->cc_win;
        g->phase = PHASE_IDLE;
        return false;
    }
    return true;
}

/* --- Persistenz ------------------------------------------------------ */
/* Kompaktes, versioniertes Format. */
#define SC_SAVE_MAGIC 0x5C11u
int sc_serialize(const SCGame *g, uint8_t *buf, int buflen) {
    int need = 2 + 4*5; /* magic + credits, wallet, bet, cherry_pot, spins */
    if (buflen < need) return 0;
    int i = 0;
    buf[i++] = (SC_SAVE_MAGIC >> 8) & 0xFF;
    buf[i++] = SC_SAVE_MAGIC & 0xFF;
    int32_t vals[5] = { g->credits, g->wallet, g->bet, g->cherry_pot, (int32_t)g->spins_total };
    for (int k = 0; k < 5; k++) {
        buf[i++] = (vals[k] >> 24) & 0xFF;
        buf[i++] = (vals[k] >> 16) & 0xFF;
        buf[i++] = (vals[k] >> 8) & 0xFF;
        buf[i++] = vals[k] & 0xFF;
    }
    return i;
}

bool sc_deserialize(SCGame *g, const uint8_t *buf, int buflen) {
    if (buflen < 22) return false;
    uint16_t magic = (uint16_t)((buf[0] << 8) | buf[1]);
    if (magic != SC_SAVE_MAGIC) return false;
    int i = 2;
    int32_t vals[5];
    for (int k = 0; k < 5; k++) {
        vals[k] = (int32_t)(((uint32_t)buf[i] << 24) | ((uint32_t)buf[i+1] << 16) |
                            ((uint32_t)buf[i+2] << 8) | (uint32_t)buf[i+3]);
        i += 4;
    }
    g->credits    = vals[0];
    g->wallet     = vals[1];
    g->bet        = vals[2] >= 1 && vals[2] <= SC_MAX_BET ? vals[2] : SC_DEFAULT_BET;
    g->cherry_pot = vals[3];
    g->spins_total= (uint32_t)vals[4];
    return true;
}

/* --- Animations-Backup ----------------------------------------------- */
static Symbol  s_backup[SC_REELS][3];
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
