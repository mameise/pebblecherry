/*
 * sc_symbols.h — zeichnet die Super-Cherry-Symbole mit Pebble-Grafikprimitiven.
 * Aufloesungsunabhaengig: jedes Symbol wird in eine GRect-Zelle gezeichnet.
 */
#ifndef SC_SYMBOLS_H
#define SC_SYMBOLS_H
#include <pebble.h>
#include "sc_core.h"

/* Zeichnet Symbol s zentriert in cell. fg/accent sind Vorder-/Akzentfarbe. */
void sc_draw_symbol(GContext *ctx, Symbol s, GRect cell);

#endif
