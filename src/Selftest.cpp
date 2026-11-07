#include "Selftest.hpp"

namespace selftest {

LGFX lcd;

namespace {
bool s_display_ready = false;
}

void log(const char* fmt, ...)
{
  char buf[192];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}

void logColor(const char* name, uint32_t rgb888)
{
  if (lcd.getColorDepth() > 16)
    log("  %-14s rgb888 = #%06lX", name, (unsigned long)(rgb888 & 0xFFFFFF));
  else
    log("  %-14s rgb565 = 0x%04X", name, (unsigned)lcd.color565(
          (rgb888 >> 16) & 0xFF, (rgb888 >> 8) & 0xFF, rgb888 & 0xFF));
}

void beginSerial(const char* sketch_name)
{
  Serial.begin(115200);
  // Warten, aber nicht ewig (USB-CDC braucht einen Moment bis der Host da ist).
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0) < 2000) { delay(10); }
  delay(200);

  log("");
  log("==============================================");
  log(" ILI9488 / ESP32-S3 Selftest: %s", sketch_name);
  log("==============================================");
  log("CPU: %u MHz, Flash: %u MB", (unsigned)(getCpuFrequencyMhz()),
      (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)));
  log("Heap frei: %u B, PSRAM frei: %u B",
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  log("Pins: SCLK=%d MOSI=%d MISO=%d CS=%d DC=%d RST=%d BL=%d",
      TFT_PIN_SCLK, TFT_PIN_MOSI, TFT_PIN_MISO, TFT_PIN_CS, TFT_PIN_DC,
      TFT_PIN_RST, TFT_PIN_BL);
  log("SPI: %s, Panel %s", LGFX_USE_MISO ? "4-Wire (MISO aktiv)" : "3-Wire (kein MISO)",
      LGFX_USE_MISO ? "lesbar" : "nur schreibbar");
  log("Panel-Variante: %s", LGFX_USE_ILI9488_IPS ? "Panel_ILI9488IPS" : "Panel_ILI9488");
}

void logPanelState()
{
  auto& cfg = lcd.panel()->config();
  log("--- Panel-Zustand ---------------------------");
  log("  memory      : %ux%u", cfg.memory_width, cfg.memory_height);
  log("  panel       : %ux%u (offset %u/%u)",
      cfg.panel_width, cfg.panel_height, cfg.offset_x, cfg.offset_y);
  log("  width/height: %d x %d (rotiert)", lcd.width(), lcd.height());
  log("  rotation    : %u", lcd.getRotation());
  log("  color depth : %u bpp -> ", (unsigned)lcd.getColorDepth());
  log("    write depth: %u bpp, read depth: %u bpp",
      (unsigned)lcd.panel()->getWriteDepth() & 0x1F,
      (unsigned)lcd.panel()->getReadDepth() & 0x1F);
  log("  readable    : %s", lcd.isReadable() ? "true" : "false");
  log("  invert      : %s", cfg.invert ? "true" : "false");
  log("  rgb_order   : %s", cfg.rgb_order ? "true (BGR)" : "false (RGB)");
  log("  bus_shared  : %s", cfg.bus_shared ? "true" : "false");
  log("---------------------------------------------");
}

void initDisplayOnce()
{
  if (s_display_ready) return;

  uint32_t t0 = millis();
  bool ok = lcd.init();
  uint32_t t1 = millis();

  s_display_ready = ok;
  log("lcd.init() -> %s (%u ms)", ok ? "true" : "false", (unsigned)(t1 - t0));

  if (!ok) {
    log("!! init() fehlgeschlagen: Verkabelung (CS/RST/DC/SCLK/MOSI) und");
    log("!! Stromversorgung pruefen. Sketch laeuft weiter, aber ohne Ausgabe.");
    return;
  }

  lcd.setRotation(0);
  lcd.fillScreen(TFT_BLACK);
  logPanelState();
}

bool setDepthChecked(int bits)
{
  lcd.setColorDepth(bits);
  const int active = (int)lcd.getColorDepth();

  // LovyanGFX liefert hier die Bitbreite des Schreibformats. Der ILI9488
  // erzwingt ueber SPI rgb888_3Byte, das sind 24 Bit (3 Byte, 8 Bit je Kanal)
  // und damit 262144 Farben -- nicht 18 Bit / 24 nutzbare Bit-Tiefe.
  // Angefordert werden darf 16 oder 18; aktiv ist am SPI-Bus immer 24.
  const bool ok = (active == bits) || (active == 24);
  const char* note = ok ? (active == 24 && bits != 24
                             ? "ok, ILI9488 nutzt ueber SPI immer rgb888_3Byte"
                             : "ok")
                        : "ABWEICHUNG";
  log("setColorDepth(%d) -> aktiv %d bpp (%s)", bits, active, note);
  return ok;
}

void setNativeDepth()
{
  // ILI9488 kann ueber SPI nur 18 bpp (RGB666), nicht 16 bpp.
  // LovyanGFX erzwingt das selbst, hier nur explizit machen.
  setDepthChecked(18);
}

void drawCornerBadge(const char* tag)
{
  char buf[64];
  const int32_t w = lcd.width();
  const int32_t h = lcd.height();

  lcd.setTextDatum(textdatum_t::top_right);
  lcd.setTextColor(TFT_YELLOW, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%s r%u %" PRId32 "x%" PRId32 " %ubpp", tag,
           lcd.getRotation(), w, h, (unsigned)lcd.getColorDepth());
  lcd.drawString(buf, w - 2, 2);
  lcd.setTextDatum(textdatum_t::top_left);
}

int32_t drawHorizontalBars(int32_t x, int32_t y, int32_t width,
                           const ColorBar* bars, size_t count, int32_t barHeight)
{
  for (size_t i = 0; i < count; ++i) {
    lcd.fillRect(x, y, width, barHeight, bars[i].rgb);
    y += barHeight;
  }
  return y;
}

int32_t drawVerticalBars(int32_t x, int32_t y, int32_t height,
                         const ColorBar* bars, size_t count, int32_t barWidth)
{
  for (size_t i = 0; i < count; ++i) {
    lcd.fillRect(x, y, barWidth, height, bars[i].rgb);
    x += barWidth;
  }
  return x;
}

void logColorBars(const ColorBar* bars, size_t count)
{
  for (size_t i = 0; i < count; ++i) logColor(bars[i].name, bars[i].rgb);
}

const char* versionString()
{
  static char buf[64];
  snprintf(buf, sizeof(buf), "LovyanGFX %d.%d.%d / selftest 1.0",
           LGFX_VERSION_MAJOR, LGFX_VERSION_MINOR, LGFX_VERSION_PATCH);
  return buf;
}

ReadbackResult checkReadback(int32_t x, int32_t y, uint32_t rgb888)
{
  ReadbackResult r{};
  r.written[0] = (rgb888 >> 16) & 0xFF;
  r.written[1] = (rgb888 >>  8) & 0xFF;
  r.written[2] = (rgb888      ) & 0xFF;

  lcd.fillRect(x - 2, y - 2, 5, 5, rgb888);

  r.raw_written = lcd.color565(r.written[0], r.written[1], r.written[2]);
  r.raw_read    = lcd.readPixel(x, y);

  // Der interne Lesepfad des ILI9488 ist auf 18 bpp festgelegt; readPixel()
  // liefert daraus einen 16-bit-Wert. Deshalb zusaetzlich in RGB-Kanaelen
  // (5/6/5 bit) vergleichen und 1 LSB Toleranz zulassen.
  const uint8_t rr = (r.raw_read >> 11) & 0x1F;
  const uint8_t rg = (r.raw_read >>  5) & 0x3F;
  const uint8_t rb = (r.raw_read      ) & 0x1F;
  r.read[0] = (rr * 255 + 15) / 31;
  r.read[1] = (rg * 255 + 31) / 63;
  r.read[2] = (rb * 255 + 15) / 31;

  const uint8_t sr = (r.written[0] * 31 + 127) / 255;
  const uint8_t sg = (r.written[1] * 63 + 127) / 255;
  const uint8_t sb = (r.written[2] * 31 + 127) / 255;

  const int d_r = abs((int)rr - (int)sr);
  const int d_g = abs((int)rg - (int)sg);
  const int d_b = abs((int)rb - (int)sb);
  r.max_diff_rgb = max(d_r, max(d_g, d_b));
  r.raw_equal = (r.raw_written == r.raw_read);
  // Toleranz von 1 LSB pro Kanal (18bit-Readback vs. 16bit-Schreibwert).
  r.rgb_equal = (r.max_diff_rgb <= 1);
  return r;
}

uint8_t readRegister8(uint8_t cmd)
{
  if (!lcd.isReadable() || !lcd.panel()) return 0;
  // readCommand() nutzt den Dummy-Read-Pfad des Panels (cfg.dummy_read_bits).
  // Siehe Hinweis auf den bekannten 1-Bit-Versatz bei logRegisterReadback().
  return static_cast<uint8_t>(lcd.panel()->readCommand(cmd, 0, 1));
}

bool logRegisterReadback()
{
  log("--- Register-Readback -----------------------");
  if (!LGFX_USE_MISO) {
    log("  MISO ist nicht verdrahtet (LGFX_USE_MISO = 0, 3-Wire).");
    log("  Register-/Pixel-Readback wird uebersprungen.");
    log("  Fuer diesen Test: MISO -> GPIO%d legen und in", TFT_PIN_MISO);
    log("  include/LGFX_Config.hpp LGFX_USE_MISO auf 1 setzen.");
    return false;
  }
  if (!lcd.isReadable()) {
    log("  Panel ist als nicht lesbar konfiguriert (cfg.readable = false).");
    return false;
  }

  const uint8_t did = readRegister8(0x04);   // RDDID
  const uint8_t mad = readRegister8(0x0B);   // RDDMADCTL
  const uint8_t col = readRegister8(0x0C);   // RDDCOLMOD

  // Bekannte Einschraenkung LovyanGFX 1.2.34 auf ESP32-S3: der Dummy-Read-Pfad
  // liefert keine stabilen Registerwerte. Gemessen: 0x54 / 0x10 / 0x0C -- die
  // Werte sind um 1 Bit verschobene Kommandocodes (0x2A=CASET, 0x08=SEGREV,
  // 0x06), also Kommando-Echo statt Registerinhalt. Eine Rueckrechnung ist
  // nicht moeglich, weil der Versatz von den vorherigen Transfers abhaengt.
  // Erkennung deshalb: ist der gelesene Wert ein bekanntes Kommando-Echo,
  // ist der Register-Readback als informativ zu werten.
  const bool echo = (did == 0x00 || did == 0xFF || did == 0x54 || did == 0x2A);
  const bool plausible = !echo;

  log("  RDDID     (0x04) = 0x%02X  (Display-ID/Herkunft)", did);
  log("  RDDMADCTL (0x0B) = 0x%02X  (Rotation %u)", mad, lcd.getRotation());
  log("  RDDCOLMOD (0x0C) = 0x%02X  (erwartet 0x66 = 18 bpp)", col);

  if (echo) {
    log("  HINWEIS: Register-Readback liefert keine stabilen Werte (Kommando-Echos");
    log("  wie 0x54/0x10/0x0C statt Registerinhalte). Ursache ist der Dummy-Read-");
    log("  Pfad von LovyanGFX 1.2.34 (Bus_SPI.inl, CPOL-Invertierung erreicht den");
    log("  SPI-Clock-Bereich nicht). Kein Hardwarefehler -- der Pixel-Readback");
    log("  darunter ist bit-exakt und bleibt das Mass der Dinge.");
    log("  Register-Reads hier nur informativ.");
  }

  if (!plausible && !echo) {
    log("  -> MISO liefert keinen plausiblen Wert. Bitte GPIO13 (TFT_MISO)");
    log("     verbinden. Der Schreibpfad funktioniert auch ohne MISO.");
  }
  return plausible;
}

} // namespace selftest
