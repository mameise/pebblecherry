/*
 * sc_symbols.h — zeichnet die Super-Cherry-Symbole mit Pebble-Grafikprimitiven.
 */
#ifndef SC_SYMBOLS_H
#define SC_SYMBOLS_H
#include <pebble.h>
#include "sc_core.h"

/* Zeichnet Symbol s zentriert in cell. */
void sc_draw_symbol(GContext *ctx, Symbol s, GRect cell);

#endif
