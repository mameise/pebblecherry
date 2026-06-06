/*
 * sc_core.h — Super Cherry Spiellogik (plattformunabhaengig)
 *
 * Originalgetreuer Nachbau der Schweizer Super-Cherry-Mechanik
 * (Golden Games / Greentube-Novomatic). Die Versionen 600/1000/2000/5000
 * unterscheiden sich nur im Maximalgewinn — die Spielmechanik ist identisch.
 * Dieser Core bildet die echte Mechanik ab; der Maximalmultiplikator ist
 * ueber SC_VERSION_MAX konfigurierbar.
 *
 * Reiner C-Core ohne Pebble-Abhaengigkeiten -> 1:1 nach Python portierbar.
 *
 * ECHTE MECHANIK (recherchiert, mehrfach quellenbestaetigt):
 *  - 3 Walzen, 1 Gewinnlinie (Mitte), 3x3 sichtbar.
 *  - Gewinn bei 3 gleichen Symbolen auf der Linie.
 *  - Kirsche zahlt schon bei 1 (linke/rechte Walze) oder 2 Kirschen.
 *  - Bei 2 gleichen auf der Linie wird ZUFAELLIG Hold ODER Step ausgeloest
 *    (keine Spielerentscheidung — das ist der Kern).
 *      * Hold: dritte Walze dreht per Re-Spin neu.
 *      * Step: dritte Walze rueckt schrittweise weiter bis Treffer/Ende.
 *  - Cherry Step: erscheint im Step eine Kirsche (und die anderen sind
 *    weder BAR noch Glocke), wird die Kombi zu 3 Kirschen, Gewinn x20.
 *  - Star Feature: 3 Sterne sichtbar -> 1 von 5 Bonusspielen.
 *    3 Sterne direkt auf der Linie -> zusaetzlich x100 Sofortgewinn.
 *  - Nach JEDEM Gewinn: Gamble-Wahl (nehmen / 2x / Mystery / Split).
 *      * Gamble 2x: 50/50 verdoppeln oder alles weg.
 *      * Gamble Mystery: zufaelliger Multiplikator x0, x2, x3, x5.
 *      * Gamble Split: Haelfte sichern, andere Haelfte bei 2x riskieren.
 *
 * Nur Spielgeld ("Credits"). Kein echtes Geld.
 */
#ifndef SC_CORE_H
#define SC_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* --- Symbole (nach echter Gewinntabelle sortiert, niedrig -> hoch) --- */
typedef enum {
    SYM_GRAPE = 0,   /* Traube  — x2  */
    SYM_PEAR,        /* Birne   — x2  */
    SYM_LEMON,       /* Zitrone — x5  */
    SYM_ORANGE,      /* Orange  — x5  */
    SYM_PLUM,        /* Pflaume — x10 */
    SYM_MELON,       /* Melone  — x10 */
    SYM_CHERRY,      /* Kirsche — x20 (Sonderregel bei 1/2) */
    SYM_BELL,        /* Glocke  — x50 */
    SYM_STAR,        /* Stern   — x100 + Star Feature */
    SYM_BAR,         /* BAR     — x500 (hoechstes Symbol) */
    SYM_COUNT
} Symbol;

/* --- Spielphasen ----------------------------------------------------- */
typedef enum {
    PHASE_IDLE = 0,    /* bereit, wartet auf Start */
    PHASE_SPIN,        /* Walzen drehen (Frontend-Animation) */
    PHASE_HOLD,        /* Hold-Feature: 3. Walze Re-Spin */
    PHASE_STEP,        /* Step-Feature: 3. Walze rueckt weiter */
    PHASE_EVAL,        /* Gewinn anzeigen */
    PHASE_GAMBLE,      /* Gamble-Menue (2x / Mystery / Split / nehmen) */
    PHASE_GAMBLE_2X,   /* 2x laeuft (Aufdecken) */
    PHASE_MYSTERY,     /* Mystery-Multiplikator wird aufgedeckt */
    PHASE_STAR         /* Star Feature Bonusspiel */
} Phase;

/* --- Was bei 2 gleichen zufaellig ausgeloest wurde ------------------- */
typedef enum { FEATURE_NONE = 0, FEATURE_HOLD, FEATURE_STEP } FeatureKind;

/* --- Gamble-Auswahl -------------------------------------------------- */
typedef enum {
    GAMBLE_PICK_NONE = 0,
    GAMBLE_PICK_TAKE,     /* Gewinn nehmen */
    GAMBLE_PICK_2X,       /* verdoppeln */
    GAMBLE_PICK_MYSTERY,  /* x0/x2/x3/x5 */
    GAMBLE_PICK_SPLIT     /* Haelfte sichern, Haelfte riskieren */
} GamblePick;

/* --- Star-Feature-Bonusspiele (1 von 5) ------------------------------ */
typedef enum {
    STAR_FRUIT_STOP = 0,  /* zufaellige Frucht bildet Gewinnlinie */
    STAR_SHUFFLE_2X,      /* 2 Respins mit garantiertem Gewinn */
    STAR_SHUFFLE_4X,      /* 4 Respins */
    STAR_10X,             /* Sofort x10 */
    STAR_CHERRY_COLLECT   /* Kirschen sammeln bis Versions-Maximum */
} StarGame;

/* --- Konstanten ------------------------------------------------------ */
#define SC_REELS          3
#define SC_REEL_LEN       16
#define SC_DEFAULT_BET    1
#define SC_MAX_BET        10
#define SC_START_CREDITS  100

/* Maximalmultiplikator je Version (600/1000/2000/5000).
 * Standard hier: 600er. Aendern fuer andere Versionen. */
#ifndef SC_VERSION_MAX
#define SC_VERSION_MAX    600
#endif

#define SC_STEP_MAX_NUDGES 6   /* wie weit die 3. Walze maximal rueckt */
#define SC_GAMBLE_MAX_ROUNDS 10

/* --- Spielzustand ---------------------------------------------------- */
typedef struct {
    Symbol window[SC_REELS][3];   /* [walze][reihe], reihe 1 == Gewinnlinie */
    int32_t reel_pos[SC_REELS];   /* aktuelle Bandposition je Walze */

    Phase  phase;
    FeatureKind feature;          /* aktives Auto-Feature (Hold/Step) */
    int32_t step_nudges_left;     /* verbleibende Step-Schritte */
    bool    cherry_step;          /* Step ist zu Cherry Step geworden */

    int32_t credits;
    int32_t bet;
    int32_t last_win;             /* aktueller (gamble-faehiger) Gewinn */
    int32_t base_win;             /* Gewinn vor Gamble (fuer Split-Logik) */

    /* Gamble */
    bool    can_gamble;
    int32_t gamble_secured;       /* bei Split gesicherter Teil */
    int32_t gamble_round;         /* Anzahl Gamble-Runden */
    int32_t mystery_mult;         /* zuletzt aufgedeckter Mystery-Wert */

    /* Star Feature */
    bool     star_pending;
    StarGame star_game;
    int32_t  star_win;

    uint32_t rng_state;
    uint32_t spins_total;
} SCGame;

/* --- API ------------------------------------------------------------- */
void    sc_init(SCGame *g, uint32_t seed);
void    sc_cycle_bet(SCGame *g);

/* Startet einen frischen Spin (zieht Einsatz ab). Befuellt alle 3 Walzen.
 * Rueckgabe false bei zu wenig Credits. Setzt Phase auf SPIN. */
bool    sc_spin(SCGame *g);

/* Nach der Spin-Animation aufrufen. Prueft die Linie:
 *  - 3 gleich / Kirschen / Sterne -> direkt Gewinn (PHASE_EVAL).
 *  - genau 2 gleich -> loest zufaellig Hold ODER Step aus
 *    (Phase wird PHASE_HOLD / PHASE_STEP), Frontend animiert dann
 *    und ruft sc_resolve_hold() bzw. sc_step_advance() auf.
 *  - sonst kein Gewinn -> PHASE_EVAL mit last_win 0.
 * Setzt g->feature entsprechend. */
void    sc_after_spin(SCGame *g);

/* Hold aufloesen: dritte (nicht-passende) Walze neu drehen, dann werten. */
void    sc_resolve_hold(SCGame *g);

/* Einen Step-Schritt ausfuehren. Rueckgabe true, wenn noch Schritte
 * folgen (Frontend soll erneut aufrufen); false wenn Step beendet.
 * Wertet bei Treffer oder Ende automatisch aus. */
bool    sc_step_advance(SCGame *g);

/* Auswertung der Gewinnlinie -> schreibt last_win/base_win, credits,
 * setzt can_gamble & star_pending. Auch direkt nutzbar. */
int32_t sc_evaluate(SCGame *g);

/* Gamble-Auswahl treffen. Bei TAKE wird gutgeschrieben & Phase IDLE.
 * Bei 2X/MYSTERY/SPLIT wird die jeweilige Unterphase gesetzt. */
void    sc_gamble_pick(SCGame *g, GamblePick pick);

/* Gamble 2x aufdecken: choice 0/1 (z.B. linke/rechte Haelfte / Farbe).
 * 50/50. Bei Gewinn verdoppelt, sonst (riskierter Teil) verloren. */
bool    sc_gamble_2x_reveal(SCGame *g, int choice);

/* Mystery aufdecken: zieht x0/x2/x3/x5 und wendet ihn an. */
int32_t sc_mystery_reveal(SCGame *g);

/* Star-Feature-Bonusspiel ausspielen -> schreibt star_win, gutschreiben. */
int32_t sc_play_star(SCGame *g);

/* Auszahlung fuer 3 gleiche Symbole (× Einsatz-Faktor, ohne bet). */
int32_t sc_payout_three(Symbol s);

/* --- Animations-Backup (Frontend ueberschreibt window beim Drehen) --- */
void    sc_save_result(SCGame *g);
void    sc_restore_result(SCGame *g);

#endif /* SC_CORE_H */
