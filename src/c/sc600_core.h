/*
 * sc600_core.h — Super Cherry 600 Spiellogik (plattformunabhaengig)
 *
 * Reiner C-Core ohne Pebble-Abhaengigkeiten. Identische Logik laesst sich
 * 1:1 nach Python (RPi Tamagotchi / PiTama) portieren.
 *
 * Modell: 3 Walzen, 1 Gewinnlinie (Mitte), 9 sichtbare Symbole.
 * Features: Hold, Step, Gamble (2x/4x), Cherry Pot.
 * Nur Spielgeld ("Credits") — kein echtes Geld.
 */
#ifndef SC600_CORE_H
#define SC600_CORE_H

#include <stdint.h>
#include <stdbool.h>

/* --- Symbole ---------------------------------------------------------
 * Reihenfolge bestimmt auch die Walzenbaender-Indizes.
 * KIRSCHE ist Sonderfall: zahlt schon bei 1x auf der Linie und fuellt den Pot.
 */
typedef enum {
    SYM_CHERRY = 0,   /* Kirsche  — Sonderzahlung + Cherry Pot */
    SYM_LEMON,        /* Zitrone  */
    SYM_ORANGE,       /* Orange   */
    SYM_PLUM,         /* Pflaume  */
    SYM_GRAPE,        /* Traube   */
    SYM_MELON,        /* Melone   */
    SYM_BELL,         /* Glocke   */
    SYM_BAR,          /* BAR      */
    SYM_SEVEN,        /* 7        */
    SYM_STAR,         /* Stern    — hoechstes Symbol, x600 */
    SYM_COUNT
} Symbol;

/* --- Spielphasen ----------------------------------------------------- */
typedef enum {
    PHASE_IDLE = 0,   /* bereit, wartet auf Start */
    PHASE_SPIN,       /* Walzen drehen (Animation laeuft im Frontend) */
    PHASE_HOLD,       /* Spieler darf Walzen halten (taktische Wahl) */
    PHASE_STEP,       /* Step/Nudge laeuft */
    PHASE_EVAL,       /* Gewinn wird ausgewertet/angezeigt */
    PHASE_GAMBLE,     /* Risikoleiter: Gewinn verdoppeln/vervierfachen */
    PHASE_POT_PAYOUT  /* Cherry Pot wird ausgeschuettet */
} Phase;

/* --- Gamble-Ergebnis ------------------------------------------------- */
typedef enum {
    GAMBLE_NONE = 0,
    GAMBLE_WON,
    GAMBLE_LOST
} GambleResult;

/* --- Konstanten ------------------------------------------------------ */
#define SC_REELS          3
#define SC_REEL_LEN       16     /* Symbole pro Walzenband */
#define SC_MAX_MULTI      600    /* x600 — Namensgeber der 600er */
#define SC_POT_THRESHOLD  20     /* Pot-Ausschuettung bei dieser Fuellung */
#define SC_POT_PER_CHERRY 1      /* +1 Pot-Einheit je sichtbarer Kirsche */
#define SC_DEFAULT_BET    1
#define SC_MAX_BET        5      /* CHF-Anlehnung: 1..5 */
#define SC_START_CREDITS  100

/* --- Spielzustand ---------------------------------------------------- */
typedef struct {
    /* Walzen: window[reel][row], row 1 == Gewinnlinie (Mitte) */
    Symbol window[SC_REELS][3];

    bool   hold[SC_REELS];     /* welche Walzen gehalten werden */
    Phase  phase;

    int32_t credits;           /* Spielguthaben (Spielgeld) */
    int32_t bet;               /* aktueller Einsatz 1..SC_MAX_BET */
    int32_t last_win;          /* Gewinn der letzten Auswertung */
    int32_t cherry_pot;        /* interner Sammeltopf (0..SC_POT_THRESHOLD) */

    bool    step_armed;        /* Step kann ausgeloest werden */
    bool    cherry_step;       /* Cherry waehrend Step -> Bonus */
    bool    can_gamble;        /* aktueller Gewinn ist verdoppelbar */
    int32_t gamble_stake;      /* Betrag auf der Risikoleiter */
    int32_t gamble_step;       /* Stufe auf der Leiter (0..) */
    GambleResult last_gamble;

    uint32_t rng_state;        /* xorshift32 Zustand */
    uint32_t spins_total;      /* Statistik */
} SC600Game;

/* --- API ------------------------------------------------------------- */

/* Initialisiert ein frisches Spiel. seed != 0. */
void sc600_init(SC600Game *g, uint32_t seed);

/* Erhoeht den Einsatz (zyklisch 1->2->..->MAX->1). */
void sc600_cycle_bet(SC600Game *g);

/* Startet einen Spin. Zieht bet von credits ab (sofern moeglich).
 * Befuellt window[] neu (gehaltene Walzen bleiben). Setzt Phase.
 * Rueckgabe: true wenn Spin gestartet, false bei zu wenig Credits. */
bool sc600_spin(SC600Game *g);

/* Toggelt Hold fuer eine Walze (nur in PHASE_HOLD sinnvoll). */
void sc600_toggle_hold(SC600Game *g, int reel);

/* Loest das Step/Nudge-Feature aus (nur wenn step_armed). */
void sc600_do_step(SC600Game *g);

/* Wertet die Gewinnlinie aus, schreibt last_win, aktualisiert credits,
 * fuellt den Cherry Pot, setzt can_gamble. Rueckgabe: Gewinnbetrag. */
int32_t sc600_evaluate(SC600Game *g);

/* Schuettet den Cherry Pot aus, wenn Schwelle erreicht. Rueckgabe: Betrag. */
int32_t sc600_collect_pot(SC600Game *g);

/* Gamble: rote/schwarze Karte raten. choice 0 oder 1.
 * Bei Gewinn verdoppelt sich gamble_stake, sonst verloren. */
GambleResult sc600_gamble(SC600Game *g, int choice);

/* Gamble-Gewinn einsammeln (zurueck zu credits). */
void sc600_gamble_collect(SC600Game *g);

/* Auszahlungs-Multiplikator fuer 3 gleiche Symbole eines Typs. */
int32_t sc600_payout_three(Symbol s);

/* Kurzname/Glyph fuer ein Symbol (1 Zeichen, ASCII). */
char sc600_symbol_glyph(Symbol s);

/* Sichert/restauriert das aktuelle Walzenbild — fuer Spin-Animationen,
 * bei denen das Frontend das Fenster voruebergehend ueberschreibt. */
void sc600_save_result(SC600Game *g);
void sc600_restore_result(SC600Game *g);

#endif /* SC600_CORE_H */
