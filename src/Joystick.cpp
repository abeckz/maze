#include "Joystick.hpp"

namespace joystick {

namespace {
int s_centerX = 2048;
int s_centerY = 2048;
int s_halfSpanX = 1024;  // Default: halber Hub, falls keine Range gesetzt wird
int s_halfSpanY = 1024;

int readAveragedRaw(int pin, uint8_t samples)
{
    if (samples == 0) samples = 1;
    int32_t sum = 0;
    for (uint8_t i = 0; i < samples; ++i) {
        sum += analogRead(pin);
    }
    return static_cast<int>(sum / samples);
}

/// Normiert eine Achse: Abstand zur Mitte, Totzone, dann durch den halben Hub,
/// danach auf [-1, +1] begrenzt.
float normalizeAxis(int raw, int center, int halfSpan)
{
    int d = raw - center;
    if (abs(d) < DEADBAND) d = 0;
    // Guard gegen Division durch Null / unsinnige Werte.
    if (halfSpan < 1) halfSpan = 1024;
    return constrain((float)d / (float)halfSpan, -1.0f, 1.0f);
}
} // namespace

float Sample::normX() const { return normalizeAxis(rawX, centerX, halfSpanX); }
float Sample::normY() const { return normalizeAxis(rawY, centerY, halfSpanY); }

void begin()
{
    // ESP32-S3: analogRead() nutzt intern adc_oneshot (ADC1 fuer diese Pins).
    analogReadResolution(ADC_RESOLUTION);               // 12 bit -> 0..4095
    analogSetPinAttenuation(JOY_PIN_VX, (adc_attenuation_t)ADC_ATTENUATION);
    analogSetPinAttenuation(JOY_PIN_VY, (adc_attenuation_t)ADC_ATTENUATION);
    // Kurze Stabilisierungszeit fuer den ADC-Sar-Takt.
    delay(50);
}

void setRange(int halfX, int halfY)
{
    if (halfX >= 1) s_halfSpanX = halfX;
    if (halfY >= 1) s_halfSpanY = halfY;
}

void calibrate(void (*logFn)(const char*))
{
    int64_t sumX = 0, sumY = 0;
    uint32_t n = 0;
    uint32_t t0 = millis();

    while ((millis() - t0) < CALIBRATION_MS) {
        sumX += analogRead(JOY_PIN_VX);
        sumY += analogRead(JOY_PIN_VY);
        ++n;
        delay(5);
    }

    s_centerX = (n > 0) ? (int)(sumX / n) : 2048;
    s_centerY = (n > 0) ? (int)(sumY / n) : 2048;

    if (logFn) {
        char buf[128];
        snprintf(buf, sizeof(buf),
                 "Joystick-Mitte kalibriert: X=%d Y=%d (%u Samples) | halfSpan X=%d Y=%d",
                 s_centerX, s_centerY, (unsigned)n, s_halfSpanX, s_halfSpanY);
        logFn(buf);
    }
}

Sample read(uint8_t samples)
{
    Sample s;
    s.rawX      = readAveragedRaw(JOY_PIN_VX, samples);
    s.rawY      = readAveragedRaw(JOY_PIN_VY, samples);
    s.centerX   = s_centerX;
    s.centerY   = s_centerY;
    s.halfSpanX = s_halfSpanX;
    s.halfSpanY = s_halfSpanY;
    return s;
}

} // namespace joystick

