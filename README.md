# Maze – Zufalls-Labyrinth mit Joystick-Steuerung (ESP32-S3 + ILI9488)

Ein kleines Spiel für ein **320x480-ILI9488-TFT** an einem **ESP32-S3** mit
16 MB Flash / 8 MB PSRAM (N16R8), angesteuert per SPI über **LovyanGFX**.

Bei jedem Start wird per **Recursive Backtracker** (randomisierte DFS) ein
neues, garantiert lösbares Labyrinth erzeugt. Der Spieler (rotes Quadrat,
Start unten links) muss zum Ziel (grünes Quadrat, oben rechts) gelangen –
gesteuert mit einem Poti-Joystick. Wird das Ziel erreicht, entsteht sofort ein
neues Labyrinth.

![Badge](https://img.shields.io/badge/platform-PlatformIO-orange)
![Badge](https://img.shields.io/badge/board-ESP32--S3%20N16R8-blue)
![Badge](https://img.shields.io/badge/display-ILI9488%20320x480-lightgrey)

## Inhalt

- [Funktionsweise](#funktionsweise)
- [Hardware](#hardware)
- [Voraussetzungen](#voraussetzungen)
- [Bauen & Flashen](#bauen--flashen)
- [Verzeichnisstruktur](#verzeichnisstruktur)
- [Maze-Algorithmus](#maze-algorithmus)
- [Joystick & Kalibrierung](#joystick--kalibrierung)
- [Fehlerbilder & Troubleshooting](#fehlerbilder--troubleshooting)
- [Lizenz](#lizenz)

---

## Funktionsweise

1. **Start** – Serial wird initialisiert, dann folgt die **Joystick-Selbstkalibrierung**
   (Stick 2 s in Ruhe/Mitte lassen). Auf dem Display erscheint „Kalibriere Joystick…“.
2. **Generierung** – anschließend wird mit dem Seed aus `esp_random()` ein neues
   Labyrinth erzeugt. Jeder Reset / jedes gelöste Spiel liefert ein **anderes**
   Labyrinth.
3. **Spiel** – der Stick bewegt den Spieler **zellweise**. Eine Wand blockiert;
   ein Impuls = eine Zelle (Debounce via Flankenerkennung).
4. **Ziel erreicht** – „Geschafft! Neues Labyrinth…“ erscheint, nach 1,2 s wird
   ein neues Labyrinth erzeugt.

Der Spieler ist das **rote** Quadrat (Start unten links), das Ziel das **grüne**
Quadrat (oben rechts).

## Hardware

| Signal | GPIO | Hinweis |
|---|---|---|
| SCLK | 12 | SPI2_HOST (FSPI), Mode 0 |
| MOSI | 11 | |
| MISO | 13 | nur für Register-/Pixel-Readback nötig (`LGFX_USE_MISO`) |
| CS   | 10 | |
| DC   | 5  | |
| RST  | 4  | |
| BL   | –1 | fest auf 3V3; in `include/LGFX_Config.hpp` auf einen GPIO setzbar |

**Joystick (Poti-Stick, ADC1):**

| Kanal | GPIO | Wertebereich |
|---|---|---|
| VX | 1 (ADC1_CH0) | 0..4095 (12 bit) |
| VY | 2 (ADC1_CH1) | 0..4095 (12 bit) |

> Die Pins stehen ausschließlich in `include/Joystick.hpp` (die einzige Stelle
> zum Anpassen), analog zu den Konventionen in `include/LGFX_Config.hpp`.

## Voraussetzungen

- [PlatformIO](https://platformio.org/) (Core) bzw. die PlatformIO-Erweiterung für VS Code
- Board: ESP32-S3 (DevKitC-1, N16R8-Modul mit OPI-Flash/PSRAM)
- Display: ILI9488 (SPI), 320x480
- Poti-Joystick an ADC1 (GPIO1/GPIO2)

## Bauen & Flashen

```powershell
# Abhängigkeiten installieren + bauen
pio run

# Flashen (COM-Port ggf. in platformio.ini anpassen, Default COM8)
pio run -t upload

# Seriellen Monitor öffnen (115200 Baud)
pio device monitor
```

Der Build läuft im **Debug-Modus** (`-O0 -g2`), damit Backtraces über den
`esp32_exception_decoder` exakt auf die richtige Zeile zeigen.

## Verzeichnisstruktur

```
platformio.ini              Konfiguration (nur env:maze)
include/LGFX_Config.hpp     Pins + SPI-Bus + Panel (einzige Stelle zum Anpassen)
include/Selftest.hpp        gemeinsame Helfer (Log, Display-Init, Readback)
include/Joystick.hpp        Poti-Joystick-Treiber (Pins + Kalibrierung)
src/main.cpp                das Maze-Spiel (Draw + Generierung + Spiellogik)
src/Selftest.cpp            Implementierung der Helfer
src/Joystick.cpp            Implementierung des Joystick-Treibers
.github/workflows/build.yml CI (baut das Maze bei jedem Push)
```

## Maze-Algorithmus

Das Labyrinth entsteht mit dem **Recursive Backtracker** (randomisierte
Tiefensuche) auf einem `MAZE_COLS × MAZE_ROWS`-Gitter:

- Jede Zelle beginnt mit **allen vier Wänden** (`WALL_N | WALL_S | WALL_W | WALL_E`).
- Vom Start (0,0) wird per Tiefensuche mit einem Stack (iterativ, kein
  Rekursions-Overflow) eine zufällige Reihenfolge der Nachbarn durchlaufen
  (Fisher-Yates-Shuffle auf 4 Elementen).
- Beim Betreten einer unbesuchten Nachbarzelle wird die **gemeinsame Wand**
  zwischen aktueller und neuer Zelle entfernt.
- Ergebnis: ein **perfektes Labyrinth** – genau ein Weg zwischen je zwei Zellen,
  insbesondere garantiert lösbar vom Start (unten links) zum Ziel (oben rechts).
- Der Zufalls-Seed stammt aus `esp_random()` → jedes Spiel ist anders.

**Layout-Konstanten** (320x480 Hochformat):

| Konstante | Wert | Bedeutung |
|---|---|---|
| `MAZE_COLS` | 13 | Zellen horizontal |
| `MAZE_ROWS` | 19 | Zellen vertikal |
| `HEADER_H` | 48 | Titel-/Hinweiszeile oben |
| `CELL` | 22 | Zellgröße in Pixel (ganzzahlig für exaktes Rendering) |

Das Labyrinth (286 × 418 px) wird unter der Kopfzeile mittig zentriert.

## Joystick & Kalibrierung

- **Ruhemitte-Kalibrierung** beim Start über `CALIBRATION_MS = 2000` (Mittelwert
  der ADC-Werte). Stick dabei **nicht bewegen**.
- **Totzone** (`DEADBAND = 24` ADC-Counts) verhindert Zittern in der Mitte.
- **Normierung** auf `[-1 … +1]` je Achse; über die Schwelle `0.55` wird eine
  diskrete Richtung ausgelöst.
- **Flankenerkennung (Latch):** ein gehaltener Impuls = **eine** Zelle; erst nach
  Rückkehr in die Mitte kann erneut ausgelöst werden.
- Abtastung im `loop()` mit `delay(16)` → ca. **60 Hz**.

## Fehlerbilder & Troubleshooting

| Symptom | Wahrscheinliche Ursache |
|---|---|
| `lcd.init() -> false`, Bildschirm weiß | RST/CS/DC nicht verbunden, 3V3-Versorgung, BL nicht versorgt |
| Bootloader-Ausgabe da, danach **nichts** | `Serial` geht an USB-CDC statt CH343 → `ARDUINO_USB_CDC_ON_BOOT`/`USB_MODE` nicht setzen |
| Init `true`, Bild schwarz | Backlight nicht an; bei BL an GPIO `TFT_PIN_BL` setzen |
| Spieler bewegt sich nicht / springt | Joystick nicht an GPIO1/GPIO2, oder Kalibrierung gestört (Stick während Kalibrierung bewegt) |
| Joystick driftet in eine Richtung | Stick während der 2-s-Kalibrierung bewegt → neu starten & ruhig halten |
| Spieler hängt an der Wand / falsche Zelle | `CELL`/Offset geändert, aber nicht überall konsistent |
| Farben invertiert / blass | `LGFX_USE_ILI9488_IPS` in `include/LGFX_Config.hpp` umschalten |

## Lizenz

MIT – siehe [LICENSE](LICENSE).
