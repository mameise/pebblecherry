/*
 * sc_core.h — Super Cherry / Super Lady — vollstaendige Spiellogik.
 *
 * Plattformunabhaengig (keine Pebble-Abhaengigkeit) -> 1:1 nach Python
 * portierbar fuer den RPi-Tamagotchi.
 *
 * Verifizierte Mechanik (Golden Games Super Cherry / Sonderspiele Super Lady):
 *
 *  GRUNDSPIEL
 *   - 3 Walzen, 1 Gewinnlinie (Mitte), 3x3 sichtbar.
 *   - 3 gleiche auf der Linie = Gewinn (siehe Tabelle).
 *   - Kirsche: 1 (aussen) = x2, 2 = x4 — auch ohne 3er.
 *
 *  STOPP-LAUFLICHT (nach JEDEM Spin)
 *   - Ein Licht wandert in fester Stationsfolge ueber die Walzen zum
 *     STOP-Feld und wieder zurueck (Schleife). Es wird mit der Zeit
 *     langsamer. Der Spieler drueckt bei STOP:
 *       * lag ein Gewinn vor -> voller Gewinn wird gutgeschrieben.
 *       * kein Gewinn -> 10% des Einsatzes zurueck (Trostbetrag).
 *   - Trifft der Spieler NICHT auf STOP, gibt es nichts (Gewinn verfaellt).
 *
 *  HOLD / STEP (zufaellig bei genau 2 gleichen auf der Linie)
 *   - Hold: dritte Walze dreht per Re-Spin neu.
 *   - Step: dritte Walze rueckt schrittweise bis Treffer/Ende.
 *   - Cherry Step: Kirsche im Step (andere nicht BAR/Glocke) -> 3 Kirschen, x20.
 *
 *  STAR FEATURE / PENTAGRAM (3 Sterne sichtbar)
 *   - Pentagram mit 5 Feldern, eines wird ausgelost:
 *     Fruit Stop, 2x Shuffle, 4x Shuffle, 10x, Cherry Collect / Cherry Pot.
 *   - 3 Sterne direkt auf der Linie: zusaetzlich x100 Sofortgewinn.
 *
 *  CHERRY COLLECT (Leiter links) — als eines der Star-Bonusspiele
 *   - 7, 10 oder 15 Steps. Walzen ruecken schrittweise. Jede Kirsche auf
 *     der Linie eine Stufe hoeher: x5, x10, x15, x100, xMAX (Versions-Cap).
 *   - Bei aufgebrauchten Steps: erreichte Stufe wird ausgezahlt.
 *
 *  CHERRY POT
 *   - Sammelt alle sichtbaren Kirschen ueber Runden. Bei 3 Sternen steht
 *     der Pot als Star-Bonus zur Auswahl.
 *
 *  GAMBLE (rechts: Mystery-Ausspielung) — MEHRFACH hintereinander
 *   - Nach jedem Gewinn waehlbar (oder nehmen):
 *       * Gamble 2x: 50/50 verdoppeln oder alles weg.
 *       * Gamble Mystery: zufaellig x0, x2, x3, x5.
 *   - Nach erfolgreichem Gamble erneut waehlbar -> beliebig oft, bis
 *     genommen, verloren, oder Versions-Cap erreicht.
 *
 *  WALLET (extern, persistent) — wie Bally 493
 *   - Muenzspeicher (credits) bleibt im Automaten, persistent.
 *   - Externe Wallet (CHF). Faellt die Wallet unter 1.-, kann aufgeladen
 *     werden (Aufladen fuellt die Wallet, Einwerfen fuellt den Speicher).
 *
 * Nur Spielgeld. Kein echtes Geld.
 */
#ifndef SC_CORE_H
#define SC_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* --- Symbole (niedrig -> hoch nach Gewinntabelle) -------------------- */
typedef enum {
    SYM_GRAPE = 0,   /* Traube  x2  */
    SYM_PEAR,        /* Birne   x2  */
    SYM_LEMON,       /* Zitrone x5  */
    SYM_ORANGE,      /* Orange  x5  */
    SYM_PLUM,        /* Pflaume x10 */
    SYM_MELON,       /* Melone  x10 */
    SYM_CHERRY,      /* Kirsche x20 (Sonderregel) */
    SYM_BELL,        /* Glocke  x50 */
    SYM_STAR,        /* Stern   x100 + Star Feature */
    SYM_BAR,         /* BAR     x500 */
    SYM_COUNT
} Symbol;

/* --- Phasen ---------------------------------------------------------- */
typedef enum {
    PHASE_IDLE = 0,    /* bereit */
    PHASE_SPIN,        /* Walzen drehen (Frontend-Animation) */
    PHASE_HOLD,        /* Hold-Respin laeuft */
    PHASE_STEP,        /* Step-Nudges laufen */
    PHASE_RUNLIGHT,    /* STOPP-Lauflicht aktiv, wartet auf Spielertaste */
    PHASE_GAMBLE,      /* Mystery/2x-Wahl (nehmen / 2x / mystery) */
    PHASE_GAMBLE_2X,   /* 2x aufdecken */
    PHASE_MYSTERY,     /* Mystery aufdecken */
    PHASE_STAR,        /* Pentagram: Bonus auslosen */
    PHASE_CHERRY_COLLECT /* Cherry-Collect-Leiter laeuft */
} Phase;

typedef enum { FEATURE_NONE = 0, FEATURE_HOLD, FEATURE_STEP } FeatureKind;

/* Star-Bonusspiele (Pentagram-Felder) */
typedef enum {
    STAR_FRUIT_STOP = 0,
    STAR_SHUFFLE_2X,
    STAR_SHUFFLE_4X,
    STAR_10X,
    STAR_CHERRY_COLLECT
} StarGame;

/* --- STOPP-Lauflicht Stationen --------------------------------------
 * Position als (reel, row): reel 0..2, row 0=oben 1=mitte 2=unten.
 * STOP ist eine Sonderstation (reel=3).
 */
#define RL_STOP_REEL  3
typedef struct { int8_t reel; int8_t row; } RLStation;

/* Voller Schleifenpfad (Hin- und Rueckweg), siehe sc_core.c. */
#define RL_PATH_LEN  16

/* --- Konstanten ------------------------------------------------------ */
#define SC_REELS          3
#define SC_REEL_LEN       16
#define SC_DEFAULT_BET    1
#define SC_MAX_BET        10

#ifndef SC_VERSION_MAX
#define SC_VERSION_MAX    600   /* 600/1000/2000/5000 */
#endif

#define SC_STEP_MAX_NUDGES 8
#define SC_POT_THRESHOLD   20   /* Cherry-Pot Sammelziel */

/* Wallet (in Rappen gerechnet, 100 = CHF 1.-) */
#define SC_COIN_VALUE      10   /* 1 Credit = 10 Rappen Spielgeld-Aequivalent */
#define SC_WALLET_START    2000 /* CHF 20.- Startwallet (in Rappen) */
#define SC_WALLET_LOW      100  /* unter CHF 1.- -> aufladbar */
#define SC_WALLET_TOPUP    2000 /* Aufladbetrag (in Rappen) */
#define SC_INSERT_COST     100  /* 1 Muenze einwerfen kostet CHF 1.- */
#define SC_INSERT_CREDITS  10   /* und gibt 10 Credits in den Speicher */

/* --- Cherry-Collect-Leiter Stufen ------------------------------------ */
/* x5, x10, x15, x100, xMAX (MAX = Versions-Cap) */
#define CC_LEVELS 5

/* --- Spielzustand ---------------------------------------------------- */
typedef struct {
    Symbol  window[SC_REELS][3];
    int32_t reel_pos[SC_REELS];

    Phase   phase;
    FeatureKind feature;
    int32_t step_nudges_left;
    bool    cherry_step;

    /* Konto / Wallet */
    int32_t credits;       /* Muenzspeicher (Spielcredits) */
    int32_t wallet;        /* externe Wallet in Rappen */
    int32_t bet;
    int32_t pending_win;    /* Gewinn, der ueber das Lauflicht zu kassieren ist */
    int32_t last_win;       /* zuletzt gutgeschriebener Gewinn (Anzeige) */

    /* STOPP-Lauflicht */
    int32_t rl_index;       /* aktuelle Station im Pfad */
    int32_t rl_ticks;       /* Ticks seit Start (fuer Verlangsamung) */
    bool    rl_won_spin;    /* lag ein Gewinn vor? */

    /* Gamble (mehrfach) */
    bool    can_gamble;
    int32_t gamble_round;
    int32_t mystery_mult;

    /* Star / Pentagram */
    bool     star_pending;
    StarGame star_game;
    int32_t  star_win;
    int32_t  pentagram_pick;   /* welches der 5 Felder leuchtet (Anim) */

    /* Cherry Collect */
    int32_t cc_steps_left;
    int32_t cc_level;          /* 0..CC_LEVELS */
    int32_t cc_win;

    /* Cherry Pot */
    int32_t cherry_pot;        /* gesammelte Kirschen */

    uint32_t rng_state;
    uint32_t spins_total;
} SCGame;

/* --- API ------------------------------------------------------------- */
void    sc_init(SCGame *g, uint32_t seed);
void    sc_cycle_bet(SCGame *g);

/* Wallet/Muenzen */
bool    sc_insert_coin(SCGame *g);   /* 1 Muenze einwerfen (Wallet -> Speicher) */
void    sc_cashout(SCGame *g);       /* Speicher -> Wallet (auszahlen) */
bool    sc_wallet_low(const SCGame *g);
void    sc_wallet_topup(SCGame *g);  /* Wallet aufladen (nur wenn low) */

/* Spin & Folgephasen */
bool    sc_spin(SCGame *g);
void    sc_after_spin(SCGame *g);    /* entscheidet Hold/Step/direkt -> dann Lauflicht */
void    sc_resolve_hold(SCGame *g);
bool    sc_step_advance(SCGame *g);

/* STOPP-Lauflicht */
void    sc_runlight_start(SCGame *g);
void    sc_runlight_tick(SCGame *g); /* eine Station weiter */
int32_t sc_runlight_tick_ms(const SCGame *g); /* aktuelle Tickdauer (wird groesser) */
bool    sc_runlight_at_stop(const SCGame *g);
void    sc_runlight_press(SCGame *g);/* Spieler drueckt: kassieren falls auf STOP */
void    sc_runlight_station(const SCGame *g, int *reel, int *row); /* aktuelle Pos */

/* Auswertung */
int32_t sc_evaluate_line(SCGame *g); /* nur Liniengewinn berechnen (kein Konto) */

/* Gamble (mehrfach) */
void    sc_gamble_take(SCGame *g);
void    sc_gamble_start_2x(SCGame *g);
void    sc_gamble_start_mystery(SCGame *g);
bool    sc_gamble_2x_reveal(SCGame *g, int choice);
int32_t sc_mystery_reveal(SCGame *g);

/* Star / Pentagram */
void    sc_star_spin_pentagram(SCGame *g); /* lost Feld aus */
int32_t sc_star_play(SCGame *g);           /* spielt ausgelostes Feld */

/* Cherry Collect */
void    sc_cc_start(SCGame *g);
bool    sc_cc_step(SCGame *g);   /* ein Step; true wenn weitere folgen */

/* Hilfen */
int32_t sc_payout_three(Symbol s);

/* Persistenz: serialisiert relevante Felder in/aus einem Puffer.
 * Rueckgabe: Anzahl geschriebener/gelesener Bytes. */
int     sc_serialize(const SCGame *g, uint8_t *buf, int buflen);
bool    sc_deserialize(SCGame *g, const uint8_t *buf, int buflen);

/* Animations-Backup */
void    sc_save_result(SCGame *g);
void    sc_restore_result(SCGame *g);

#endif /* SC_CORE_H */
