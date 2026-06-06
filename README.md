# Super Cherry — Pebble-Watchapp 🍒

Originalgetreuer Nachbau des Schweizer Kult-Spielautomaten **Super Cherry**
(Golden Games / Greentube-Novomatic) fuer die **Pebble Time 2 (emery)**,
kompatibel mit basalt / chalk / diorite / aplite.

> Nur **Spielgeld** ("Credits"). Kein echtes Geld, kein Gluecksspiel.

## Versionen 600 / 1000 / 2000 / 5000

Die Versionen unterscheiden sich nur im **Maximalgewinn**, die Spielmechanik
ist identisch. Standard ist hier die **600er** (Max x600). Fuer eine andere
Version beim Bauen einfach das Define setzen, z.B.:

```c
#define SC_VERSION_MAX 5000   // vor #include "sc_core.h", oder in package/wscript
```

## Echte Mechanik

Anders als beim ersten Entwurf bildet diese Version die *tatsaechliche*
Super-Cherry-Mechanik ab (quellenbestaetigt, u.a. jackpots.ch, pasino.ch,
casino.ch):

- **3 Walzen, 1 Gewinnlinie** (Mitte), 3×3 sichtbar.
- **Gewinn bei 3 gleichen Symbolen** auf der Linie.
- **Kirschen-Sonderregel:** 1 Kirsche (links/rechts) zahlt x2, 2 Kirschen x4.
- **Hold ODER Step — automatisch & zufaellig:** Bei genau **2 gleichen**
  auf der Linie loest der Automat selbst *zufaellig* eines der beiden aus
  (keine Spielerentscheidung — das ist der Kern der echten Mechanik):
  - **Hold:** die dritte Walze dreht per Re-Spin neu.
  - **Step:** die dritte Walze rueckt schrittweise weiter, bis sie trifft
    oder die Schritte aufgebraucht sind.
- **Cherry Step:** taucht im Step eine Kirsche auf (und die anderen Symbole
  sind weder BAR noch Glocke), formen die Walzen automatisch 3 Kirschen → x20.
- **Star Feature:** 3 Sterne sichtbar → eines von 5 Bonusspielen
  (Fruit Stop, 2x Shuffle, 4x Shuffle, 10x, Cherry Collect).
  3 Sterne direkt auf der Linie geben zusaetzlich x100.
- **Gamble nach JEDEM Gewinn** (drei echte Optionen, wie am Original):
  - **Gamble 2x:** verdoppeln — oder alles weg (50/50).
  - **Gamble Mystery:** zufaelliger Multiplikator x0, x2, x3 oder x5.
  - **Gamble Split:** Haelfte sichern, andere Haelfte bei 2x riskieren.
  - Bis zu 10 Gamble-Runden, gedeckelt auf das Versions-Maximum.

### Gewinntabelle (3 gleiche × Einsatz)

| Symbol  | Faktor | Symbol  | Faktor |
|---------|--------|---------|--------|
| BAR     | x500   | Melone  | x10    |
| Stern   | x100   | Pflaume | x10    |
| Glocke  | x50    | Orange  | x5     |
| Kirsche | x20    | Zitrone | x5     |
|         |        | Birne   | x2     |
|         |        | Traube  | x2     |

Kirsche einzeln (aussen): x2 · 2 Kirschen: x4

## Steuerung (3-Tasten-Schema, an PiTama angelehnt)

| Phase        | UP                | SELECT          | DOWN              |
|--------------|-------------------|-----------------|-------------------|
| Bereit       | —                 | **Spin**        | Einsatz +1        |
| (Hold/Step laufen automatisch ab — keine Eingabe noetig)               |
| Gewinn/Gamble| Gamble 2x         | Gewinn nehmen   | Gamble Mystery    |
|              | (UP lang = Split) |                 |                   |
| 2x aufdecken | linkes Feld       | —               | rechtes Feld      |
| Star Feature | —                 | Bonus spielen   | —                 |

## Bauen

### CloudPebble (am einfachsten)

1. <https://cloudpebble.repebble.com> → neues C-Projekt
2. `sc_core.h/.c`, `sc_symbols.h/.c`, `main.c` einfuegen
3. Plattform **emery** waehlen → Run in Emulator / auf die Uhr installieren

### Lokales SDK

```bash
uv tool install pebble-tool --python 3.13
pebble sdk install latest
pebble build
pebble install --emulator emery
```

## Projektstruktur

```
supercherry/
├── package.json
├── README.md
└── src/c/
    ├── sc_core.h / sc_core.c     # echte Spiellogik (plattformunabhaengig)
    ├── sc_symbols.h / sc_symbols.c  # gezeichnete Symbole (GPath/Primitive)
    └── main.c                    # Pebble-UI, Phasen, Animationen, Buttons
```

## Symbole

Alle Symbole sind als Vektorgrafik direkt gezeichnet (kein PNG-Resource noetig):
Kirsche (zwei Beeren + Blatt), Glocke, blaues BAR-Schild, gelber 5-Zacken-Stern
(GPath), Traubencluster, Melone (rotes Innenleben), sowie Birne (eifoermig) und
die runden Fruechte Zitrone/Orange/Pflaume mit Blattstiel und Glanzpunkt.

## Portierung auf den RPi-Tamagotchi

`sc_core.h/.c` ist komplett Pebble-frei und 1:1 nach Python portierbar
(gleiche Funktionsnamen, gleicher Zustand). Fuer den Pi (ST7789 240×240,
3 Buttons, Piezo) wird nur ein neues Frontend gebraucht — analog zum
Bally-493-Core in PiTama.

---
🍒 Hinweis: Gluecksspiel kann suechtig machen. Dies ist ein nostalgischer
Spielgeld-Nachbau ohne echten Geldeinsatz. Bei Problemen: SOS Spielsucht 0800 040 080.
