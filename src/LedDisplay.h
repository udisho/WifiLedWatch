#ifndef LED_DISPLAY_H
#define LED_DISPLAY_H

#include <FastLED.h>
#include "Config.h"

/*
  LED layout per digit (29 LEDs):

     11 10  9  8
     12          7
     13          6
     14          5
     15          4
  [16] 0  1  2  3     <-- LED 16 is wiring-only, NEVER lit
     17          28
     18          27
     19          26
     20          25
       21 22 23 24

  Colon/seconds LEDs are separate dumb LEDs on COLON_LED_PIN (GPIO 23)
*/

#define LED_UNUSED 999

class LedDisplay {
public:
    void begin();

    void showNumber(int number);
    void renderNumber(int number);  // fill LED buffer without showing
    void forceShow();               // explicit safeShow()
    void showNumberAnimated(int number);
    void showNumberFadeAnimated(int number, bool allDigits = false);
    void showCONN();
    void scrollCONN();  // one frame of scrolling "CONN" right-to-left
    void showIP(const char* ip);
    void scrollText(const char* text, int delayMs);
    void clear();

    // Animations
    void runStartupAnimation();
    void runCascadeAnimation();
    void showCrazy();
    void showPulse();

    // Color
    void setColor(CRGB color);
    void setColorByIndex(int index);
    void nextColor();
    int  getColorIndex() const { return m_colorIndex; }
    CRGB getColor() const { return m_color; }
    int  getColorCount() const;
    const char* getColorName(int index) const;

    void setOverrideColor(CRGB color) { m_overrideColor = color; m_useOverride = true; }
    void clearOverrideColor() { m_useOverride = false; }
    bool hasOverrideColor() const { return m_useOverride; }

    void setBrightness(uint8_t brightness);
    uint8_t getBrightness() const { return m_brightness; }

    // Pause pulse: call repeatedly in loop, returns true while pulsing
    void pulseBrightness();

    CRGB activeColor() const { return m_useOverride ? m_overrideColor : m_color; }

private:
    CRGB m_leds[TOTAL_LEDS];
    CRGB m_color = CRGB::LightGreen;
    CRGB m_overrideColor = CRGB::Black;
    bool m_useOverride = false;
    int  m_colorIndex = 0;
    uint8_t m_brightness = DEFAULT_BRIGHTNESS;
    int  m_lastDisplayed = -1;

    void showOneDigit(int digitPos, int charIndex);
    int  charToPattern(char c);
    void safeShow();
    void wipeDigitTransition(int digitPos, int oldChar, int newChar);
};

struct ColorEntry {
    CRGB color;
    const char* name;
};

extern const ColorEntry COLOR_TABLE[];
extern const int COLOR_COUNT;

#endif // LED_DISPLAY_H
