#include "LedDisplay.h"
#include <Arduino.h>

// Segment groups: top=8,9,10,11 UL=12,13,14,15 UR=4,5,6,7 mid=0,1,2,3 LL=17,18,19,20 LR=25,26,27,28 bot=21,22,23,24
#define PAT_COUNT 31
static const unsigned int DIGIT_PATTERNS[PAT_COUNT][28] = {
    // 0-9: digits
    { 11, 10,  9,  8,  7,  6,  5,  4, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 15, 14, 13, 12, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 0
    {  4,  5,  6,  7, 28, 27, 26, 25, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 1
    { 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, 17, 18, 19, 20, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 2
    { 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0, 28, 27, 26, 25, 24, 23, 22, 21, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 3
    { 12, 13, 14, 15,  0,  1,  2,  3,  4,  5,  6,  7, 28, 27, 26, 25, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 4
    {  8,  9, 10, 11, 12, 13, 14, 15,  0,  1,  2,  3, 28, 27, 26, 25, 24, 23, 22, 21, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 5
    {  8,  9, 10, 11, 12, 13, 14, 15,  0,  1,  2,  3, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 6
    { 11, 10,  9,  8,  7,  6,  5,  4, 28, 27, 26, 25, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 7
    {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28 }, // 8
    { 21, 22, 23, 24, 25, 26, 27, 28,  3,  2,  1,  0,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // 9
    // 10-16: legacy CONN chars
    {  8,  9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // C (10)
    {  0,  1,  2,  3, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // O (11) - also used for D(0) display
    {  0,  1,  2,  3, 17, 18, 19, 20, 25, 26, 27, 28, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // N (12)
    {  8,  9, 10, 11, 12, 13, 14, 15,  0,  1,  2,  3, 17, 18, 19, 20, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // E (13)
    {  0,  1,  2,  3, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // t (14)
    {  4,  5,  6,  7, 28, 27, 26, 25, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // I (15)
    {  8,  9, 10, 11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // G (16)
    // 17-28: new alphabet entries
    {  8,  9, 10, 11, 12, 13, 14, 15,  4,  5,  6,  7,  0,  1,  2,  3, 17, 18, 19, 20, 25, 26, 27, 28, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // A (17) = top+UL+UR+mid+LL+LR
    {  0,  1,  2,  3, 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // b (18) = mid+UL+LL+bot+LR
    {  0,  1,  2,  3,  4,  5,  6,  7, 25, 26, 27, 28, 21, 22, 23, 24, 17, 18, 19, 20, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // d (19) = mid+UR+LR+bot+LL
    {  8,  9, 10, 11, 12, 13, 14, 15,  0,  1,  2,  3, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // F (20) = top+UL+mid
    { 12, 13, 14, 15,  4,  5,  6,  7,  0,  1,  2,  3, 17, 18, 19, 20, 25, 26, 27, 28, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // H (21) = UL+UR+mid+LL+LR
    {  4,  5,  6,  7, 25, 26, 27, 28, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // J (22) = UR+LR+bot
    { 12, 13, 14, 15, 17, 18, 19, 20, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // L (23) = UL+LL+bot
    {  8,  9, 10, 11, 12, 13, 14, 15,  4,  5,  6,  7,  0,  1,  2,  3, 17, 18, 19, 20, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // P (24) = top+UL+UR+mid+LL
    {  0,  1,  2,  3, 17, 18, 19, 20, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // r (25) = mid+LL
    { 12, 13, 14, 15,  4,  5,  6,  7, 17, 18, 19, 20, 25, 26, 27, 28, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // U (26) = UL+UR+LL+LR+bot
    { 12, 13, 14, 15,  4,  5,  6,  7,  0,  1,  2,  3, 25, 26, 27, 28, 21, 22, 23, 24, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // Y (27) = UL+UR+mid+LR+bot
    { LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // SPACE (28) = blank
    {  0,  1,  2,  3, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // MINUS (29) = mid only
    {  8,  9, 10, 11, 12, 13, 14, 15,  4,  5,  6,  7,  0,  1,  2,  3, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED, LED_UNUSED }, // DEGREE (30) = top+UL+UR+mid
};

#define CHAR_C 10
#define CHAR_O 11
#define CHAR_N 12
#define CHAR_E 13
#define CHAR_t 14
#define CHAR_I 15
#define CHAR_G 16
#define CHAR_A 17
#define CHAR_b 18
#define CHAR_d 19
#define CHAR_F 20
#define CHAR_H 21
#define CHAR_J 22
#define CHAR_L 23
#define CHAR_P 24
#define CHAR_r 25
#define CHAR_U 26
#define CHAR_Y 27
#define CHAR_SPACE 28
#define CHAR_MINUS 29
#define CHAR_DEG   30

const ColorEntry COLOR_TABLE[] = {
    { CRGB::Red, "Red" }, { CRGB::Green, "Green" }, { CRGB::Blue, "Blue" },
    { CRGB::Yellow, "Yellow" }, { CRGB::Cyan, "Cyan" }, { CRGB::Magenta, "Magenta" },
    { CRGB::Orange, "Orange" }, { CRGB::Purple, "Purple" }, { CRGB::Aqua, "Aqua" },
    { CRGB::Lime, "Lime" }, { CRGB::Indigo, "Indigo" }, { CRGB::Teal, "Teal" },
    { CRGB::Turquoise, "Turquoise" }, { CRGB::Gold, "Gold" }, { CRGB::Maroon, "Maroon" },
    { CRGB::Olive, "Olive" }, { CRGB::Navy, "Navy" }, { CRGB::SkyBlue, "SkyBlue" },
    { CRGB::Coral, "Coral" }, { CRGB::Lavender, "Lavender" }, { CRGB::Silver, "Silver" },
    { CRGB::Pink, "Pink" }, { CRGB::White, "White" },
};
const int COLOR_COUNT = sizeof(COLOR_TABLE) / sizeof(COLOR_TABLE[0]);

// Voltage drop compensation: farther digits get dimmer (especially blue).
// Proportional boost ensures channels at 0 stay at 0 (no blue into pure red).
#define VDROP_GENERAL_PER_DIGIT  10  // ~4% brightness boost per digit (in 1/256ths)
#define VDROP_BLUE_PER_DIGIT     16  // ~6% extra blue boost per digit (in 1/256ths)

static CRGB compensateVoltage(CRGB c, int digitPos) {
    if (digitPos <= 0) return c;
    uint16_t gBoost = digitPos * VDROP_GENERAL_PER_DIGIT;
    uint16_t bBoost = digitPos * VDROP_BLUE_PER_DIGIT;
    c.r = min(255, (int)(c.r + ((uint16_t)c.r * gBoost >> 8)));
    c.g = min(255, (int)(c.g + ((uint16_t)c.g * gBoost >> 8)));
    c.b = min(255, (int)(c.b + ((uint16_t)c.b * bBoost >> 8)));
    return c;
}

void LedDisplay::safeShow() {
    for (int d = 0; d < NUM_DIGITS; d++)
        m_leds[d * NUM_LEDS_PER_DIGIT + WIRING_ONLY_LED] = CRGB::Black;
    FastLED.show();
}

void LedDisplay::begin() {
    FastLED.addLeds<NEOPIXEL, LED_DATA_PIN>(m_leds, TOTAL_LEDS);
    FastLED.setBrightness(m_brightness);
    pinMode(COLON_LED_PIN, OUTPUT);
    digitalWrite(COLON_LED_PIN, LOW);
    clear();
}

void LedDisplay::clear() {
    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
    safeShow();
}

void LedDisplay::showOneDigit(int digitPos, int charIndex) {
    if (charIndex < 0 || charIndex >= PAT_COUNT) return;
    CRGB c = compensateVoltage(activeColor(), digitPos);
    int offset = digitPos * NUM_LEDS_PER_DIGIT;
    for (int i = 0; i < 28; i++) {
        unsigned int ledIdx = DIGIT_PATTERNS[charIndex][i];
        if (ledIdx == LED_UNUSED) break;
        if (ledIdx == WIRING_ONLY_LED) continue;
        if ((int)(offset + ledIdx) < TOTAL_LEDS)
            m_leds[offset + ledIdx] = c;
    }
}

void LedDisplay::renderNumber(int number) {
    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
    for (int i = NUM_DIGITS - 1; i >= 0; --i) { showOneDigit(i, number % 10); number /= 10; }
}

void LedDisplay::renderTemp(int tempC) {
    // Positive: "D D ° _" (e.g., "14° "), single digit: " 5° "
    // Negative: "- D D °" (e.g., "-14°"), single digit: "- 5 °"... actually "-5° "
    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
    bool neg = tempC < 0;
    int abs_t = neg ? -tempC : tempC;
    if (abs_t > 99) abs_t = 99;

    if (neg) {
        // -DD° or -D°_
        showOneDigit(0, CHAR_MINUS);
        if (abs_t >= 10) {
            showOneDigit(1, abs_t / 10);
            showOneDigit(2, abs_t % 10);
            showOneDigit(3, CHAR_DEG);
        } else {
            showOneDigit(1, abs_t);
            showOneDigit(2, CHAR_DEG);
        }
    } else {
        // DD°_ or _D°_
        if (abs_t >= 10) {
            showOneDigit(0, abs_t / 10);
            showOneDigit(1, abs_t % 10);
        } else {
            showOneDigit(1, abs_t);
        }
        showOneDigit(2, CHAR_DEG);
    }
}

void LedDisplay::forceShow() { safeShow(); }

void LedDisplay::showNumber(int number) {
    renderNumber(number);
    safeShow();
}

void LedDisplay::showCONN() {
    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
    static const int cc[] = { CHAR_C, CHAR_O, CHAR_N, CHAR_N };
    for (int i = 0; i < NUM_DIGITS; i++) showOneDigit(i, cc[i]);
    safeShow();
}

void LedDisplay::showNumberAnimated(int number) {
    int nw[NUM_DIGITS], ol[NUM_DIGITS];
    int t = number;
    for (int i = NUM_DIGITS - 1; i >= 0; --i) { nw[i] = t % 10; t /= 10; }
    t = m_lastDisplayed;
    for (int i = NUM_DIGITS - 1; i >= 0; --i) { ol[i] = t % 10; t /= 10; }

    bool any = false;
    for (int i = 0; i < NUM_DIGITS; i++) if (nw[i] != ol[i]) any = true;

    if (!any || m_lastDisplayed < 0) {
        fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
        for (int i = 0; i < NUM_DIGITS; i++) showOneDigit(i, nw[i]);
        safeShow();
        m_lastDisplayed = number;
        return;
    }

    for (int i = 0; i < NUM_DIGITS; i++)
        if (nw[i] != ol[i]) wipeDigitTransition(i, ol[i], nw[i]);

    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
    for (int i = 0; i < NUM_DIGITS; i++) showOneDigit(i, nw[i]);
    safeShow();
    m_lastDisplayed = number;
}

void LedDisplay::showNumberFadeAnimated(int number, bool allDigits) {
    int nw[NUM_DIGITS], ol[NUM_DIGITS];
    int t = number;
    for (int i = NUM_DIGITS - 1; i >= 0; --i) { nw[i] = t % 10; t /= 10; }
    t = m_lastDisplayed;
    for (int i = NUM_DIGITS - 1; i >= 0; --i) { ol[i] = t % 10; t /= 10; }

    bool changed[NUM_DIGITS] = {};
    bool any = false;
    for (int i = 0; i < NUM_DIGITS; i++) {
        changed[i] = allDigits || (nw[i] != ol[i]);
        if (changed[i]) any = true;
    }

    if (!any || m_lastDisplayed < 0) {
        renderNumber(number);
        safeShow();
        m_lastDisplayed = number;
        return;
    }

    CRGB c = activeColor();

    // Fade out ONLY changed digits (darken their LEDs individually)
    for (int step = 8; step >= 0; step--) {
        for (int d = 0; d < NUM_DIGITS; d++) {
            if (!changed[d]) continue;
            int offset = d * NUM_LEDS_PER_DIGIT;
            for (int led = 0; led < NUM_LEDS_PER_DIGIT; led++) {
                if (led == WIRING_ONLY_LED) continue;
                m_leds[offset + led].fadeToBlackBy(28);
            }
        }
        safeShow();
        delay(5);
    }

    // Black out changed digits, render new digits
    for (int d = 0; d < NUM_DIGITS; d++) {
        if (!changed[d]) continue;
        int offset = d * NUM_LEDS_PER_DIGIT;
        for (int led = 0; led < NUM_LEDS_PER_DIGIT; led++) {
            if (led == WIRING_ONLY_LED) continue;
            m_leds[offset + led] = CRGB::Black;
        }
        showOneDigit(d, nw[d]);
    }

    // Fade in changed digits (brighten from dim to full)
    // First dim the new digits
    for (int d = 0; d < NUM_DIGITS; d++) {
        if (!changed[d]) continue;
        int offset = d * NUM_LEDS_PER_DIGIT;
        for (int led = 0; led < NUM_LEDS_PER_DIGIT; led++) {
            if (led == WIRING_ONLY_LED) continue;
            m_leds[offset + led].fadeToBlackBy(240);
        }
    }

    for (int step = 0; step < 9; step++) {
        for (int d = 0; d < NUM_DIGITS; d++) {
            if (!changed[d]) continue;
            int offset = d * NUM_LEDS_PER_DIGIT;
            // Re-render at increasing brightness
            for (int led = 0; led < NUM_LEDS_PER_DIGIT; led++) {
                if (led == WIRING_ONLY_LED) continue;
                m_leds[offset + led] = CRGB::Black;
            }
            showOneDigit(d, nw[d]);
            uint8_t fade = 255 - (step * 28);
            for (int led = 0; led < NUM_LEDS_PER_DIGIT; led++) {
                if (led == WIRING_ONLY_LED) continue;
                m_leds[offset + led].fadeToBlackBy(fade);
            }
        }
        safeShow();
        delay(5);
    }

    // Final clean render
    renderNumber(number);
    safeShow();
    m_lastDisplayed = number;
}

void LedDisplay::pulseBrightness() {
    uint8_t origBright = m_brightness;
    // Dim down
    for (int b = origBright; b >= PAUSE_PULSE_MIN_BRIGHT; b -= 3) {
        FastLED.setBrightness(b);
        safeShow();
        delay(PAUSE_PULSE_SPEED_MS);
    }
    FastLED.setBrightness(PAUSE_PULSE_MIN_BRIGHT);
    safeShow();
    // Dim up
    for (int b = PAUSE_PULSE_MIN_BRIGHT; b <= origBright; b += 3) {
        FastLED.setBrightness(b);
        safeShow();
        delay(PAUSE_PULSE_SPEED_MS);
    }
    FastLED.setBrightness(origBright);
    safeShow();
}

void LedDisplay::wipeDigitTransition(int digitPos, int oldChar, int newChar) {
    int offset = digitPos * NUM_LEDS_PER_DIGIT;
    CRGB c = activeColor();
    static const int rows[][8] = {
        { 8, 9, 10, 11, -1, -1, -1, -1 },
        { 4, 5, 6, 7, 12, 13, 14, 15 },
        { 0, 1, 2, 3, -1, -1, -1, -1 },
        { 17, 18, 19, 20, 25, 26, 27, 28 },
        { 21, 22, 23, 24, -1, -1, -1, -1 },
    };
    static const int rs[] = { 4, 8, 4, 8, 4 };

    for (int r = 0; r < 5; r++) {
        for (int j = 0; j < rs[r]; j++) {
            int l = rows[r][j];
            if (l >= 0 && l != WIRING_ONLY_LED) m_leds[offset + l] = CRGB::Black;
        }
        safeShow(); delay(TRANSITION_STEP_MS);
    }

    bool on[NUM_LEDS_PER_DIGIT] = {};
    for (int i = 0; i < 28; i++) {
        unsigned int idx = DIGIT_PATTERNS[newChar][i];
        if (idx == LED_UNUSED) break;
        if (idx < NUM_LEDS_PER_DIGIT && idx != WIRING_ONLY_LED) on[idx] = true;
    }

    for (int r = 0; r < 5; r++) {
        for (int j = 0; j < rs[r]; j++) {
            int l = rows[r][j];
            if (l >= 0 && l != WIRING_ONLY_LED && on[l]) m_leds[offset + l] = c;
        }
        safeShow(); delay(TRANSITION_STEP_MS);
    }
}

void LedDisplay::runStartupAnimation() {
    CRGB c = activeColor();
    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
    for (int i = 0; i < TOTAL_LEDS; i++) {
        if ((i % NUM_LEDS_PER_DIGIT) == WIRING_ONLY_LED) continue;
        m_leds[i] = c;
        safeShow(); delay(STARTUP_LED_DELAY_MS);
    }
    // Flash colon LEDs during test too
    digitalWrite(COLON_LED_PIN, HIGH); delay(200);
    digitalWrite(COLON_LED_PIN, LOW);

    for (int d = 0; d <= 9; d++) {
        fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
        for (int pos = 0; pos < NUM_DIGITS; pos++) showOneDigit(pos, d);
        safeShow(); delay(STARTUP_DIGIT_DELAY_MS);
    }

    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black); safeShow(); delay(100);
    for (int pos = 0; pos < NUM_DIGITS; pos++) showOneDigit(pos, 8);
    safeShow(); delay(300);
    clear();
}

void LedDisplay::runCascadeAnimation() {
    for (int round = 0; round < 3; round++) {
        for (int pos = 0; pos < NUM_DIGITS; pos++) {
            fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
            CRGB saved = m_color; bool so = m_useOverride;
            m_color = CHSV((round * 60 + pos * 40) % 256, 255, 255); m_useOverride = false;
            showOneDigit(pos, 8);
            m_color = saved; m_useOverride = so;
            safeShow(); delay(80);
        }
    }
    for (int i = 0; i < 40; i++) {
        int led = random(TOTAL_LEDS);
        if ((led % NUM_LEDS_PER_DIGIT) == WIRING_ONLY_LED) continue;
        m_leds[led] = CHSV(random(256), 255, 255);
        if (i % 3 == 0) for (int j = 0; j < TOTAL_LEDS; j++) m_leds[j].fadeToBlackBy(60);
        safeShow(); delay(30);
    }
    for (int d = 0; d <= 9; d++) {
        for (int pos = 0; pos < NUM_DIGITS; pos++) {
            fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
            for (int p2 = 0; p2 <= pos; p2++) showOneDigit(p2, (d + NUM_DIGITS - p2) % 10);
            safeShow(); delay(40);
        }
    }
    clear();
}

void LedDisplay::showIP(const char* ip) {
    // Parse IP octets
    int octets[4] = {0, 0, 0, 0};
    int idx = 0;
    for (const char* p = ip; *p && idx < 4; p++) {
        if (*p == '.') { idx++; }
        else if (*p >= '0' && *p <= '9') { octets[idx] = octets[idx] * 10 + (*p - '0'); }
    }

    // Build a sequence of digits to scroll: e.g. "10.0.0.9"
    // Each digit is a number 0-9 or -1 for dot (shown via colon LED)
    int seq[20]; // max: 3+1+3+1+3+1+3 = 15 entries
    int len = 0;

    for (int o = 0; o < 4; o++) {
        if (o > 0) seq[len++] = -1;  // dot separator
        if (octets[o] >= 100) seq[len++] = octets[o] / 100;
        if (octets[o] >= 10)  seq[len++] = (octets[o] / 10) % 10;
        seq[len++] = octets[o] % 10;
    }

    // Helper lambda to scroll the sequence once
    auto scrollOnce = [&]() {
        int padded = len + 4 + 4;
        for (int pos = 0; pos < padded; pos++) {
            fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
            bool colonOn = false;
            for (int d = 0; d < NUM_DIGITS; d++) {
                int seqIdx = pos - (NUM_DIGITS - 1 - d);
                if (seqIdx >= 0 && seqIdx < len) {
                    if (seq[seqIdx] == -1) colonOn = true;
                    else showOneDigit(d, seq[seqIdx]);
                }
            }
            digitalWrite(COLON_LED_PIN, colonOn ? HIGH : LOW);
            safeShow();
            delay(350);
        }
    };

    // First pass
    scrollOnce();

    // Separator animation: flash all segments briefly
    for (int i = 0; i < 3; i++) {
        fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
        for (int d = 0; d < NUM_DIGITS; d++) showOneDigit(d, 8);
        safeShow(); delay(150);
        clear(); delay(150);
    }

    // Second pass
    scrollOnce();

    digitalWrite(COLON_LED_PIN, LOW);
    clear();
}

// Scrolling "CONNECtING" animation — call repeatedly, advances one step per call
static int scrollConnPos = 0;
void LedDisplay::scrollCONN() {
    // C-O-N-N-E-C-t-I-N-G
    static const int chars[] = { CHAR_C, CHAR_O, CHAR_N, CHAR_N, CHAR_E, CHAR_C, CHAR_t, CHAR_I, CHAR_N, CHAR_G };
    static const int charLen = 10;
    int totalLen = charLen + NUM_DIGITS + NUM_DIGITS;  // pad before and after

    fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
    for (int d = 0; d < NUM_DIGITS; d++) {
        int charIdx = scrollConnPos - (NUM_DIGITS - 1 - d);
        if (charIdx >= 0 && charIdx < charLen) {
            showOneDigit(d, chars[charIdx]);
        }
    }
    safeShow();

    scrollConnPos++;
    if (scrollConnPos >= totalLen) scrollConnPos = 0;
}

int LedDisplay::charToPattern(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') c -= 32;  // to uppercase
    switch (c) {
        case 'A': return CHAR_A;
        case 'B': return CHAR_b;
        case 'C': return CHAR_C;
        case 'D': return CHAR_d;
        case 'E': return CHAR_E;
        case 'F': return CHAR_F;
        case 'G': return CHAR_G;
        case 'H': return CHAR_H;
        case 'I': return CHAR_I;
        case 'J': return CHAR_J;
        case 'K': return CHAR_H;  // approximation
        case 'L': return CHAR_L;
        case 'M': return CHAR_N;  // approximation
        case 'N': return CHAR_N;
        case 'O': return CHAR_O;
        case 'P': return CHAR_P;
        case 'Q': return 9;       // looks like 9
        case 'R': return CHAR_r;
        case 'S': return 5;       // looks like 5
        case 'T': return CHAR_t;
        case 'U': return CHAR_U;
        case 'V': return CHAR_U;  // approximation
        case 'W': return CHAR_U;  // approximation
        case 'X': return CHAR_H;  // approximation
        case 'Y': return CHAR_Y;
        case 'Z': return 2;       // looks like 2
        case ' ': return CHAR_SPACE;
        default:  return CHAR_SPACE;
    }
}

void LedDisplay::scrollText(const char* text, int delayMs) {
    int len = strlen(text);
    int padded = len + NUM_DIGITS + NUM_DIGITS;
    for (int pos = 0; pos < padded; pos++) {
        fill_solid(m_leds, TOTAL_LEDS, CRGB::Black);
        for (int d = 0; d < NUM_DIGITS; d++) {
            int charIdx = pos - (NUM_DIGITS - 1 - d);
            if (charIdx >= 0 && charIdx < len) {
                int pat = charToPattern(text[charIdx]);
                if (pat >= 0) showOneDigit(d, pat);
            }
        }
        safeShow();
        delay(delayMs);
    }
}

void LedDisplay::showCrazy() {
    // Always recolor lit LEDs (prevents static-color flash on digit change).
    // Only regenerate random hues every 200ms for a calm animation rate.
    static unsigned long lastUpdate = 0;
    static uint8_t hues[TOTAL_LEDS] = {};
    unsigned long now = millis();
    if (now - lastUpdate >= 180) {  // slightly faster than display refresh to avoid drift stutter
        lastUpdate = now;
        for (int i = 0; i < TOTAL_LEDS; i++) hues[i] = random(256);
    }
    for (int i = 0; i < TOTAL_LEDS; i++) {
        if ((i % NUM_LEDS_PER_DIGIT) == WIRING_ONLY_LED) continue;
        if (m_leds[i]) {
            CRGB c = CHSV(hues[i], 255, 255);
            m_leds[i] = compensateVoltage(c, i / NUM_LEDS_PER_DIGIT);
        }
    }
}

void LedDisplay::showPulse() {
    // Pulse mode: hold a color, then rapidly sweep through the gradient to the next.
    static uint8_t currentHue = 0;
    static uint8_t displayHue = 0;
    static unsigned long phaseStart = 0;
    static bool transitioning = false;

    unsigned long now = millis();
    if (phaseStart == 0) phaseStart = now;

    if (!transitioning) {
        displayHue = currentHue;
        if (now - phaseStart >= 23000) {  // hold 23 seconds
            transitioning = true;
            phaseStart = now;
        }
    } else {
        unsigned long elapsed = now - phaseStart;
        const unsigned long sweepMs = 3000;  // 3s smooth sweep
        const uint8_t hueStep = 18;          // smaller steps = more colors visited
        if (elapsed >= sweepMs) {
            currentHue += hueStep;
            displayHue = currentHue;
            transitioning = false;
            phaseStart = now;
        } else {
            // Ease-in-out: smoothstep 3t²-2t³
            uint32_t t256 = (elapsed * 256) / sweepMs;  // 0-255
            uint32_t eased = (t256 * t256 * (768 - 2 * t256)) >> 16;  // smoothstep scaled
            if (eased > 255) eased = 255;
            displayHue = currentHue + (uint8_t)(((uint32_t)hueStep * eased) >> 8);
        }
    }

    for (int i = 0; i < TOTAL_LEDS; i++) {
        if ((i % NUM_LEDS_PER_DIGIT) == WIRING_ONLY_LED) continue;
        if (m_leds[i]) {
            CRGB c = CHSV(displayHue, 255, 255);
            m_leds[i] = compensateVoltage(c, i / NUM_LEDS_PER_DIGIT);
        }
    }
}

void LedDisplay::setColor(CRGB color) {
    m_color = color;
    for (int i = 0; i < COLOR_COUNT; i++) if (COLOR_TABLE[i].color == color) { m_colorIndex = i; return; }
    m_colorIndex = -1;
}
void LedDisplay::setColorByIndex(int index) {
    if (index >= 0 && index < COLOR_COUNT) { m_colorIndex = index; m_color = COLOR_TABLE[index].color; }
}
void LedDisplay::nextColor() { m_colorIndex = (m_colorIndex + 1) % COLOR_COUNT; m_color = COLOR_TABLE[m_colorIndex].color; }
int LedDisplay::getColorCount() const { return COLOR_COUNT; }
const char* LedDisplay::getColorName(int index) const { return (index >= 0 && index < COLOR_COUNT) ? COLOR_TABLE[index].name : "Custom"; }
void LedDisplay::setBrightness(uint8_t brightness) {
    m_brightness = (brightness > MAX_BRIGHTNESS) ? MAX_BRIGHTNESS : brightness;
    FastLED.setBrightness(m_brightness);
}
