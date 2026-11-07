/**
 * LGFX_Config.hpp
 *
 * Zentrale Hardware-/Panel-Konfiguration fuer ESP32-S3 (N16R8) + ILI9488 (SPI).
 *
 * Diese Datei ist die einzige Stelle, an der Pins und Panel-Parameter
 * angepasst werden muessen. Alle Testsketche in src/ nutzen die Klasse LGFX
 * aus dieser Datei.
 */
#pragma once

#include <Arduino.h>
#include <LovyanGFX.hpp>

// ---------------------------------------------------------------------------
// Board-Variante des ILI9488-Panels
// ---------------------------------------------------------------------------
//  false -> Panel_ILI9488    (Standard-ILI9488, meist TFT-Modul, invert = false)
//  true  -> Panel_ILI9488IPS (BuyDisplay / viele "IPS 3.5 Zoll"-Module,
//                             braucht andere Gammawerte + invert = true)
// Bei falschen Farben (Negativbild / blasse Farben) hier umschalten.
#ifndef LGFX_USE_ILI9488_IPS
#define LGFX_USE_ILI9488_IPS 0
#endif

// ---------------------------------------------------------------------------
// MISO / Lesbarkeit des Panels
// ---------------------------------------------------------------------------
// 0 -> 3-Wire-Betrieb: MISO ist nicht angeschlossen, das Panel ist nicht lesbar.
//      Alle Schreibpfade funktionieren normal. Register-/Pixel-Readback,
//      readPixel() und isReadable() liefern nichts Brauchbares (0x00/0xFF).
// 1 -> 4-Wire-Betrieb: MISO muss auf TFT_PIN_MISO liegen. Erst damit sind
//      readRegister8(), readPixel() und der readback-Sketch aussagekraeftig.
//      Aktuell auf 1 gesetzt: MISO ist verdrahtet.
// Beide Flags zusammen halten spi_3wire und readable konsistent: ein offener
// MISO-Pin bei readable = true startet SPI-Transaktionen ohne Datenquelle und
// erzeugt Zufallswerte, die als Fehler fehlinterpretiert werden.
#ifndef LGFX_USE_MISO
#define LGFX_USE_MISO 1
#endif

// ---------------------------------------------------------------------------
// SPI-Bus
// ---------------------------------------------------------------------------
// TFT_SCLK -> GPIO12, TFT_MOSI -> GPIO11, TFT_CS -> GPIO10, TFT_DC -> GPIO5
// TFT_MISO -> GPIO13 (nur bei LGFX_USE_MISO = 1 anschliessen)
// Backlight-Pin: -1 = nicht ueber Software gesteuert (BL fest auf 3V3).
static constexpr int TFT_PIN_SCLK = 12;
static constexpr int TFT_PIN_MOSI = 11;
static constexpr int TFT_PIN_MISO = 13;
static constexpr int TFT_PIN_CS   = 10;
static constexpr int TFT_PIN_DC   = 5;
static constexpr int TFT_PIN_RST  = 4;
static constexpr int TFT_PIN_BL   = -1;   // z.B. 21 falls Backlight an einem GPIO haengt
// IF-16_WR (mode 0) ist das Standard-Timing des ILI9488.
// 40 MHz schreibt 320x480@18bit in ~115 ms; hoehere Werte sind moeglich,
// aber stark von Leiterbahnlaenge/Steckbrett abhaengig.
static constexpr uint32_t TFT_FREQ_WRITE = 40000000;
static constexpr uint32_t TFT_FREQ_READ  = 16000000;

// ---------------------------------------------------------------------------
// LGFX-Device
// ---------------------------------------------------------------------------
class LGFX : public lgfx::LGFX_Device
{
#if LGFX_USE_ILI9488_IPS
  lgfx::Panel_ILI9488IPS _panel_instance;
#else
  lgfx::Panel_ILI9488    _panel_instance;
#endif
  lgfx::Bus_SPI          _bus_instance;
  lgfx::Light_PWM        _light_instance;

public:
  LGFX(void)
  {
    { // ----- SPI-Bus -----
      auto cfg = _bus_instance.config();

      cfg.spi_host    = SPI2_HOST;      // ESP32-S3: SPI2_HOST (FSPI) oder SPI3_HOST
      cfg.spi_mode    = 0;              // CPOL=0, CPHA=0
      cfg.freq_write  = TFT_FREQ_WRITE;
      cfg.freq_read   = TFT_FREQ_READ;
      cfg.spi_3wire   = !LGFX_USE_MISO; // 3-Wire, solange kein MISO verdrahtet ist
      cfg.use_lock    = true;           // Transaktions-Lock aktiv
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = TFT_PIN_SCLK;
      cfg.pin_mosi = TFT_PIN_MOSI;
      cfg.pin_miso = TFT_PIN_MISO;
      cfg.pin_dc   = TFT_PIN_DC;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    { // ----- Panel -----
      auto cfg = _panel_instance.config();

      cfg.pin_cs  = TFT_PIN_CS;
      cfg.pin_rst = TFT_PIN_RST;
      cfg.pin_busy = -1;

      cfg.memory_width  = 320;          // ILI9488: 320x480 (keine Varianten wie ST7735)
      cfg.memory_height = 480;
      cfg.panel_width   = 320;
      cfg.panel_height  = 480;
      cfg.offset_x      = 0;
      cfg.offset_y      = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits  = 1;
      cfg.readable      = LGFX_USE_MISO; // nur mit angeschlossenem MISO sinnvoll
      cfg.invert        = LGFX_USE_ILI9488_IPS;   // IPS-Module sind invertiert
      cfg.rgb_order     = false;        // true, wenn Rot und Blau vertauscht sind
      cfg.dlen_16bit    = false;        // ILI9488 arbeitet ueber SPI mit 8-bit-Einheiten
      cfg.bus_shared    = false;        // keine SD-Karte am selben Bus

      _panel_instance.config(cfg);
    }

    if (TFT_PIN_BL >= 0)
    { // ----- Backlight (PWM) -----
      auto cfg = _light_instance.config();
      cfg.pin_bl      = TFT_PIN_BL;
      cfg.invert      = false;
      cfg.freq        = 44100;
      cfg.pwm_channel = 7;
      _light_instance.config(cfg);
      _panel_instance.setLight(&_light_instance);
    }

    setPanel(&_panel_instance);
  }
};
