# Super Cherry 600 — Pebble-Watchapp 🍒

Ein Nachbau des Schweizer Kult-Spielautomaten **Super Cherry 600** von Golden
Games (Peter Schorno), gebaut fuer die **Pebble Time 2 (emery)** und kompatibel
mit basalt / chalk / diorite / aplite.

> Nur **Spielgeld** ("Credits"). Kein echtes Geld, kein Gluecksspiel.
> Die historische 600er war in der Schweiz ein Geldspielautomat — dieser
> Nachbau ist reine Nostalgie/Bastelei.

## Features (Vollausbau)

Originalgetreu nach der 600er-Mechanik:

- **3 Walzen, 1 Gewinnlinie** (Mitte), 9 sichtbare Symbole
- **Symbole:** Kirsche, Zitrone, Orange, Pflaume, Traube, Melone, Glocke, BAR, 7, Stern
- **Kirschen-Sonderregel:** zahlen schon bei 1x auf der Linie
- **Hold:** Walzen halten und einmal neu drehen (das taktische "Geschicklichkeits"-Feature der 600er)
- **Step / Nudge:** bei 2 gleichen Symbolen wird Step scharf; schiebt Walzen weiter
- **Cherry Step:** Kirsche waehrend Step → Bonusgewinn
- **Cherry Pot:** jede *sichtbare* Kirsche fuellt den internen Topf (auch ausserhalb der Linie); bei voller Schwelle Ausschuettung
- **Gamble (Risikoleiter):** Gewinn verdoppeln durch Rot/Schwarz-Raten, bis x600

## Steuerung (3-Tasten-Schema, an PiTama angelehnt)

| Phase    | UP                 | SELECT (kurz)     | SELECT (lang)     | DOWN              |
|----------|--------------------|-------------------|-------------------|-------------------|
| Bereit   | Pot einsammeln*    | **Spin starten**  | —                 | Einsatz +1        |
| Halten   | Walze 1 halten     | Walze 2 halten    | **neu drehen**    | Walze 3 halten    |
| Auswert. | ins Gamble**       | weiter            | —                 | —                 |
| Gamble   | Rot tippen         | Gewinn einsammeln | —                 | Schwarz tippen    |

\* nur wenn der Cherry Pot voll ist
\** nur wenn ein Gewinn vorliegt

BACK in der Bereit-Phase verlaesst die App.

## Bauen

### Variante A — CloudPebble (am einfachsten, kein Setup)

1. Auf <https://cloudpebble.repebble.com> einloggen
2. Neues Projekt (Pebble C SDK) anlegen
3. Inhalt von `src/c/sc600_core.h`, `src/c/sc600_core.c` und `src/c/main.c` einfuegen
4. In den Projekt-Settings `package.json`-Werte (UUID, Plattformen) uebernehmen
5. Plattform **emery** waehlen, "Run in Emulator" oder auf die Uhr installieren

### Variante B — Lokales SDK

```bash
# Pebble-Tool installieren (aktuelle Rebble-Toolchain)
uv tool install pebble-tool --python 3.13
pebble sdk install latest

# Im Projektordner
pebble build
pebble install --emulator emery     # im Emulator testen
# oder auf echte Uhr:
pebble install --phone <IP>         # via Pebble-App
```

## Projektstruktur

```
supercherry600/
├── package.json          # App-Metadaten, Plattform-Targets
├── README.md
└── src/
    └── c/
        ├── sc600_core.h  # plattformunabhaengige Spiellogik (API)
        ├── sc600_core.c  # Logik-Implementierung (RNG, Auszahlung, Features)
        └── main.c        # Pebble-UI (Rendering, Buttons, Animation)
```

## Auszahlungstabelle (3 gleiche × Einsatz)

| Symbol | Faktor | Symbol  | Faktor |
|--------|--------|---------|--------|
| Stern  | x600   | Melone  | x25    |
| 7      | x150   | Traube  | x20    |
| BAR    | x80    | Pflaume | x14    |
| Glocke | x40    | Orange  | x10    |
| Kirsche (3x) | x30 | Zitrone | x8    |

Kirsche einzeln: x2 · Kirsche doppelt: x6 · Cherry-Step-Bonus: +5

## Portierung auf den RPi-Tamagotchi (naechster Schritt)

`sc600_core.{h,c}` enthaelt **keine** Pebble-Abhaengigkeit und laesst sich 1:1
nach Python uebersetzen (gleiche Funktionsnamen, gleicher Zustand). Fuer den Pi
(ST7789 240×240, 3 Buttons, Piezo) wird nur ein neues Frontend gebraucht —
analog zum Bally-493-Core in PiTama. Sag Bescheid, dann baue ich das.

---
🍒 Viel Spass — und denk dran: in echt verliert das Haus nie.
