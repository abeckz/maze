/**
 * Joystick.hpp
 *
 * Treiber fuer einen Poti-Joystick (VX/VY) am ESP32-S3.
 *
 * - Zwei Achsen werden ueber ADC1 gelesen (frei von WiFi/Bluetooth-Nutzung).
 * - Die Ruhemitte wird beim Start kalibriert (Mittelwert waehrend eines
 *   Kalibrierfensters), damit das Fadenkreuz in der Mitte steht.
 * - Auswahl der Pins + ADC-Einstellungen stehen nur hier (einzige Stelle
 *   zum Anpassen), passend zu den Konventionen von LGFX_Config.hpp.
 */
#pragma once

#include <Arduino.h>

namespace joystick {

// ---------------------------------------------------------------------------
// Pins (ADC1, GPIO des ESP32-S3)
// ---------------------------------------------------------------------------
static constexpr int JOY_PIN_VX = 1;   // ADC1_CH0  -> VX-Ausgang des Joysticks
static constexpr int JOY_PIN_VY = 2;   // ADC1_CH1  -> VY-Ausgang des Joysticks

/// Dauer des Kalibrierfensters beim Start (Stick dabei in Ruhe/Mitte halten).
static constexpr uint32_t CALIBRATION_MS = 2000;

/// Toleranzbereich um die kalibrierte Mitte, der als "neutral" gilt.
/// Verhindert Zittern des Fadenkreuzes, wenn der Stick losgelassen wird.
static constexpr int DEADBAND = 24;    // in ADC-Counts (12 bit)

/// ADC-Einstellungen (ESP32-S3)
static constexpr int ADC_RESOLUTION = 12;  // 0..4095
static constexpr int ADC_ATTENUATION = ADC_11db;  // ~0..3.3 V, fuer 3V3-VCC des Sticks

/// Ergebnis einer Achsenlesung (Rohwerte + kalibrierte normierte Werte).
struct Sample
{
    int  rawX;      ///< ADC-Rohwert X (0..4095)
    int  rawY;      ///< ADC-Rohwert Y (0..4095)
    int  centerX;   ///< kalibrierte Mitte X
    int  centerY;   ///< kalibrierte Mitte Y
    int  halfSpanX; ///< halber Auslenkungsbereich X (Abstand Mitte -> Rand)
    int  halfSpanY; ///< halber Auslenkungsbereich Y
    /// normierte Achse X: -1.0 (links) .. 0.0 (Mitte) .. +1.0 (rechts)
    float normX() const;
    /// normierte Achse Y: -1.0 (oben) .. 0.0 (Mitte) .. +1.0 (unten)
    float normY() const;
};

/// Initialisiert die ADC-Einheit (einmal aufrufen).
void begin();

/// Kalibriert die Ruhemitte ueber CALIBRATION_MS.
/// Stick dabei in Ruhe/Mitte lassen, Fortschritt geht an den Log-Callback.
/// @param logFn optionaler Callback, der jeden Zwischenstand loggt.
void calibrate(void (*logFn)(const char*) = nullptr);

/// Setzt den halben Auslenkungsbereich je Achse (Abstand Mitte -> Rand in
/// ADC-Counts). Damit laeuft der Stick auf dem Display bis an den Rand, auch
/// wenn sein Poti den vollen 12-bit-Hub mechanisch nicht erreicht.
/// Nutzt man das nicht, wird ein Default von 2048 halbiert angenommen.
/// @param halfX halber Hub X (Counts), @param halfY halber Hub Y (Counts)
void setRange(int halfX, int halfY);

/// Liest beide Achsen (mit optionalem Oversampling fuer stabilere Werte).
/// @param samples Anzahl aufeinanderfolgender Lesungen zum Mitteln.
Sample read(uint8_t samples = 8);

} // namespace joystick
