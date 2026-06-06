# Super Cherry / Super Lady — Pebble-Watchapp 🍒

Nachbau des Schweizer Kult-Spielautomaten **Super Cherry** (Golden Games),
im Stil von **Super Lady** (Sonderspiele, Winterthur), fuer die
**Pebble Time 2 (emery)** — kompatibel mit basalt / chalk / diorite / aplite.

> Nur **Spielgeld** (Credits). Kein echtes Geld, kein Gluecksspiel.

## Layout (wie Super Lady)

```
 +--------------------------------------+
 |        CR 50    FR 20.00             |  Guthaben-Display + Wallet
 | CHRY     [  Pentagram  ]      MYST  |  Cherry-Leiter | Stern | Mystery
 | MAX                            5x   |
 | 100x        ( Stern )          3x   |
 | 15x       5 Bonus-Felder       2x   |
 | 10x                            0x   |
 | 5x       +-----------+              |
 |          | W1  W2  W3|  [STOP]      |  3 Walzen + STOPP-Lauflicht
 |          +-----------+              |
 |            SELECT: Spin             |  Statuszeile
 +--------------------------------------+
```

## Features (alle verifiziert am Original)

- **3 Walzen, 1 Gewinnlinie.** 3 gleiche = Gewinn. Kirsche: 1 (aussen) = x2, 2 = x4.
- **STOPP-Lauflicht** nach jedem Spin: ein Licht wandert in fester Schleife
  (L-oben to mitte to unten to mitte to M-mitte to R-mitte to STOP und zurueck),
  wird mit der Zeit langsamer. Bei **STOP** druecken = Gewinn kassieren.
  Kein Gewinn? Trotzdem Lauflicht — bei STOP gibt's 10% Einsatz zurueck.
- **Hold / Step** (automatisch & zufaellig bei 2 gleichen), inkl. **Cherry Step** (x20).
- **Star Feature / Pentagram** (3 Sterne): eines von 5 Bonusspielen leuchtet auf —
  Fruit Stop, 2x Shuffle, 4x Shuffle, 10x, Cherry Collect.
- **Cherry Collect** (Leiter links): 7/10/15 Steps, je 3 Kirschen eine Stufe hoeher:
  x5, x10, x15, x100, xMAX. Bei Step-Ende erreichte Stufe ausgezahlt.
- **Cherry Pot**: sammelt sichtbare Kirschen ueber Runden.
- **Gamble — MEHRFACH** (rechts, Mystery-Ausspielung): nach jedem Gewinn
  **Gamble 2x** (verdoppeln/alles weg) oder **Gamble Mystery** (x0/x2/x3/x5).
  Nach erfolgreichem Gamble erneut waehlbar — beliebig oft bis genommen,
  verloren oder Versions-Cap (x600).
- **Wallet + Persistenz** (wie Bally 493): Muenzspeicher bleibt im Automaten,
  persistent ueber App-Schliessen. Auszahlen, Einwerfen, und **Auto-Aufladen**
  der Wallet wenn sie unter CHF 1.- faellt.

## Steuerung (3 Tasten, kontextabhaengig)

| Phase            | UP                  | SELECT            | DOWN            |
|------------------|---------------------|-------------------|-----------------|
| Bereit           | Muenze einwerfen *  | **Spin**          | Einsatz +1      |
|                  | (UP lang = Auszahlen)|                  |                 |
| STOPP-Lauflicht  | –                   | **Stoppen**       | –               |
| Gewinn/Gamble    | Gamble 2x           | Gewinn nehmen     | Gamble Mystery  |
| 2x aufdecken     | linkes Feld         | –                 | rechtes Feld    |
| Star Feature     | –                   | Pentagram/Bonus   | –               |
| Cherry Collect   | (laeuft automatisch)| –                 | –               |

\* Wenn die Wallet unter CHF 1.- ist, laedt UP die Wallet auf (+20.-).
   Sonst wirft UP eine Muenze ein (CHF 1.- aus Wallet -> 10 Credits).

## Gewinntabelle (3 gleiche × Einsatz)

| Symbol | Faktor | Symbol  | Faktor |
|--------|--------|---------|--------|
| BAR    | x500   | Melone  | x10    |
| Stern  | x100   | Pflaume | x10    |
| Glocke | x50    | Orange  | x5     |
| Kirsche| x20    | Zitrone | x5     |
|        |        | Birne   | x2     |
|        |        | Traube  | x2     |

## Bauen

### CloudPebble (am einfachsten)

1. <https://cloudpebble.repebble.com> -> neues C-Projekt
2. Diese Dateien einfuegen: `sc_core.h/.c`, `sc_symbols.h/.c`, `main.c`
3. Plattform **emery** -> Run in Emulator / auf die Uhr

### Lokales SDK

```bash
uv tool install pebble-tool --python 3.13
pebble sdk install latest
pebble build
pebble install --emulator emery
```

## Version einstellen (600/1000/2000/5000)

Standard ist die **600er** (Max x600). Fuer andere Versionen vor dem Bauen
in `sc_core.h` setzen, z.B.:

```c
#define SC_VERSION_MAX 5000
```

## Projektstruktur

```
supercherry/
├── package.json
├── README.md
└── src/c/
    ├── sc_core.h / sc_core.c       # Spiellogik (plattformunabhaengig)
    ├── sc_symbols.h / sc_symbols.c # gezeichnete Symbole
    └── main.c                      # UI, Layout, Phasen, Persistenz
```

## Spielstand zuruecksetzen

App deinstallieren und neu installieren — persistent storage wird geloescht.

## Portierung auf den RPi-Tamagotchi

`sc_core.h/.c` ist komplett Pebble-frei und 1:1 nach Python portierbar.
Fuer den Pi (ST7789 240x240, 3 Buttons, Piezo) wird nur ein neues Frontend
gebraucht — analog zum Bally-493-Core in PiTama. Das 240x240-Display passt
sogar besser zum dichten Super-Lady-Layout als die Pebble.

---
🍒 Gluecksspiel kann suechtig machen. Dies ist ein nostalgischer Spielgeld-
Nachbau ohne echten Geldeinsatz. Hilfe: SOS Spielsucht 0800 040 080.
