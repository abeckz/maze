#pragma once

#include <Arduino.h>
#include <inttypes.h>   // PRId32/PRId64 -- Pflicht, weil lcd.width()/height()
                        // je nach Ziel int32_t oder long sind.
#include <LovyanGFX.hpp>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "LGFX_Config.hpp"

namespace selftest {

/// Globales Display-Objekt aller Testumgebungen.
extern LGFX lcd;

/// Log-Ausgabe auf Serial (unabhaengig vom Display, damit man auch bei
/// defekter Verkabelung etwas sieht).
void log(const char* fmt, ...);

/// Hexadezimale Farbdarstellung passend zum aktuellen Farbtiefe-Modus.
void logColor(const char* name, uint32_t rgb888);

/// Serial auf 115200 + Speicheruebersicht. Am Anfang jedes Sketches aufrufen.
void beginSerial(const char* sketch_name);

/// Init + einmalige Statusausgabe: Panel-Groesse, Rotation, Farbtiefe, Pins.
void initDisplayOnce();

/// Kurze Zusammenfassung des Panel-Zustands als String (fuer die Anzeige).
void logPanelState();

/// Farbtiefe setzen und pruefen, ob sie wirklich aktiv wurde.
/// Am ILI9488 ist ueber SPI unabhaengig vom Wunsch immer rgb888_3Byte aktiv,
/// d.h. getColorDepth() meldet 24 bpp. Das gilt hier als Erfolg.
/// @return true wenn getColorDepth() der angeforderten Tiefe entspricht (oder 24).
bool setDepthChecked(int bits);

/// Farbtiefe passend zum ILI9488 setzen:
/// SPI -> 18 bpp (rgb888_3Byte), sonst 16 bpp.
void setNativeDepth();

/// Zeichnet oben rechts eine kleine Info-Ecke (Rotation / Groesse / Tiefe).
void drawCornerBadge(const char* tag);

/// Zeichnet einen vertikalen Farbbalken-Satz (RGB888-Werte) und beschriftet
/// die Sollwerte im Serial Log, damit man sie am Bildschirm abgleichen kann.
struct ColorBar
{
  const char* name;
  uint32_t    rgb;
};

/// Zeichnet horizontale Balken ueber die volle Breite ab y.
/// @return y-Position unterhalb der Balken.
int32_t drawHorizontalBars(int32_t x, int32_t y, int32_t width,
                           const ColorBar* bars, size_t count, int32_t barHeight);

/// Zeichnet vertikale Balken ab x.
/// @return x-Position rechts neben den Balken.
int32_t drawVerticalBars(int32_t x, int32_t y, int32_t height,
                         const ColorBar* bars, size_t count, int32_t barWidth);

/// Loggt alle Sollwerte einer Balkenliste.
void logColorBars(const ColorBar* bars, size_t count);

/// Version der Library + der Testumgebung als String.
const char* versionString();

/// Ergebnis eines Pixel-Readback-Tests.
/// Wichtig: der ILI9488 liest immer mit 18 bpp (RGB666) zurueck, geschrieben
/// wird im aktuellen write_depth. Deshalb wird zusaetzlich in 8-bit-Werten
/// verglichen, damit der Test unabhaengig vom Schreibmodus aussagekraeftig ist.
struct ReadbackResult
{
  bool    raw_equal;      ///< Rohwerte identisch
  bool    rgb_equal;      ///< RGB-Anteile gleich (0..31/0..63/0..31)
  int32_t max_diff_rgb;   ///< groesste Abweichung pro Kanal (0 = exakt)
  uint16_t raw_written;
  uint16_t raw_read;
  uint8_t written[3];     ///< R,G,B wie geschrieben (0..255)
  uint8_t read[3];        ///< R,G,B wie gelesen (auf 0..255 skaliert)
};

/// Schreibt einen 5x5-Block in rgb888 und liest die Mitte zurueck.
ReadbackResult checkReadback(int32_t x, int32_t y, uint32_t rgb888);

/// Liest ein 8-bit-Register des Panels (z.B. 0x04 RDDID, 0x0B RDDMADCTL,
/// 0x0C RDDCOLMOD). Gibt bei fehlender Lesbarkeit 0 zurueck.
uint8_t readRegister8(uint8_t cmd);

/// Loggt RDDID/RDDMADCTL/RDDCOLMOD und bewertet das Ergebnis.
/// @return true, wenn plausible Werte zurueckkamen (MISO funktioniert).
bool logRegisterReadback();

} // namespace selftest
