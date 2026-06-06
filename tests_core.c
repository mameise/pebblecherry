#include "src/c/sc600_core.h"
#include <stdio.h>

static void render(const SC600Game *g) {
    for (int row = 0; row < 3; row++) {
        printf("  ");
        for (int r = 0; r < SC_REELS; r++)
            printf("[%c]", sc600_symbol_glyph(g->window[r][row]));
        printf("%s\n", row == 1 ? "  <- Linie" : "");
    }
}

int main(void) {
    SC600Game g;
    sc600_init(&g, 0xABCDEF01u);
    printf("Start: credits=%d bet=%d\n", g.credits, g.bet);

    int wins = 0, steps = 0, pots = 0;
    for (int i = 0; i < 2000 && g.credits >= g.bet; i++) {
        sc600_spin(&g);
        g.phase = PHASE_HOLD;      /* Frontend wuerde hier Hold anbieten */
        g.phase = PHASE_EVAL;
        int32_t w = sc600_evaluate(&g);
        if (w > 0) wins++;
        if (g.step_armed) { steps++; sc600_do_step(&g); sc600_evaluate(&g); }
        if (g.cherry_pot >= SC_POT_THRESHOLD) {
            int32_t p = sc600_collect_pot(&g);
            if (p > 0) pots++;
        }
        g.phase = PHASE_IDLE;
    }
    printf("Nach Lauf: credits=%d spins=%u wins=%d steps=%d potPayouts=%d\n",
           g.credits, g.spins_total, wins, steps, pots);

    /* Gamble-Pfad pruefen */
    sc600_init(&g, 7u);
    g.last_win = 10; g.gamble_stake = 10; g.can_gamble = true;
    GambleResult gr = sc600_gamble(&g, 0);
    printf("Gamble result=%d stake=%d\n", gr, g.gamble_stake);

    printf("Payout-Tabelle Stichprobe: STAR=%d SEVEN=%d CHERRY=%d LEMON=%d\n",
           sc600_payout_three(SYM_STAR), sc600_payout_three(SYM_SEVEN),
           sc600_payout_three(SYM_CHERRY), sc600_payout_three(SYM_LEMON));
    render(&g);
    return 0;
}
