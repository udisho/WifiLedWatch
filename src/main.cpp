#include <Arduino.h>
#include "Config.h"
#include "LedDisplay.h"
#include "TimeManager.h"
#include "WifiManager.h"
#include "WebUI.h"
#include "ConfigStore.h"

LedDisplay   ledDisplay;
TimeManager  timeManager;
WifiManager  wifiManager;
WebUI        webUI;
ConfigStore  configStore;
WatchSettings settings;

// Timing
unsigned long lastDisplayUpdate = 0;
unsigned long lastColonToggle = 0;
bool colonState = false;
int lastClockDisplay = -1;
unsigned long lastDateShow = 0;
unsigned long lastBirthdayCheck = 0;
int lastBirthdayHour = -1;

#define CLOCK_REFRESH_MS     200
#define STOPWATCH_REFRESH_MS  50
#define TABATA_REFRESH_MS     50
#define TIMER_REFRESH_MS     100

// Colon LED helper
void updateColonLED(unsigned long now, bool wifiLost) {
    // Check if colon LEDs should be off (user disabled or night shift active)
    bool nightActive = false;
    if (settings.nightShiftEnabled && timeManager.isTimeSynced()) {
        int h = timeManager.getHours();
        if (settings.nightShiftStartHour > settings.nightShiftEndHour)
            nightActive = (h >= settings.nightShiftStartHour || h < settings.nightShiftEndHour);
        else
            nightActive = (h >= settings.nightShiftStartHour && h < settings.nightShiftEndHour);
    }
    if (!settings.colonLedsEnabled || nightActive) {
        digitalWrite(COLON_LED_PIN, LOW);
        return;
    }
    unsigned long blinkRate = wifiLost ? COLON_BLINK_FAST_MS : COLON_BLINK_NORMAL_MS;
    if (now - lastColonToggle >= blinkRate) {
        lastColonToggle = now;
        colonState = !colonState;
        digitalWrite(COLON_LED_PIN, colonState ? HIGH : LOW);
    }
}

uint8_t buzzVol() { return settings.buzzerLevel == 1 ? 40 : 128; } // low=40, high=128

void playBuzzer() {
    uint8_t v = buzzVol();
    ledcWriteTone(0, 1000); ledcWrite(0, v); delay(200);
    ledcWrite(0, 0); delay(100);
    ledcWriteTone(0, 1500); ledcWrite(0, v); delay(200);
    ledcWrite(0, 0); delay(100);
    ledcWriteTone(0, 2000); ledcWrite(0, v); delay(400);
    ledcWrite(0, 0);
}

void playPhaseBeep(bool isWork) {
    uint8_t v = buzzVol();
    ledcWriteTone(0, isWork ? 2000 : 800); ledcWrite(0, v); delay(150);
    ledcWrite(0, 0);
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n=== WiFi LED Watch v3.0 ===");
    Serial.println("By Udi & Noam Shorer");

    configStore.begin();
    configStore.load(settings);

    ledDisplay.begin();
    ledDisplay.setBrightness(settings.brightness);
    // Buzzer setup
    ledcSetup(0, 1000, 8);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, 0);  // silence immediately
    if (settings.colorIndex >= 0) ledDisplay.setColorByIndex(settings.colorIndex);
    else ledDisplay.setColor(CRGB(settings.customR, settings.customG, settings.customB));

    ledDisplay.runStartupAnimation();
    ledDisplay.showCONN();
    wifiManager.begin();

    Serial.printf("Setup complete (%lu ms)\n", millis());
}

void loop() {
    unsigned long now = millis();
    wifiManager.update();

    static bool servicesStarted = false;
    if (wifiManager.isConnected() && !servicesStarted) {
        servicesStarted = true;

        // Start web server FIRST so GUI is available immediately
        timeManager.begin(&wifiManager);
        timeManager.setTimezoneOffset(settings.timezoneOffset);
        timeManager.setDSTMode(settings.dstMode);
        timeManager.setDSTRules(settings.dstStart, settings.dstEnd);
        webUI.begin(&ledDisplay, &timeManager, &configStore, &settings, &wifiManager);

        // Show IP on LEDs after services are up
        ledDisplay.showIP(wifiManager.getIP().c_str());

        Serial.printf("Access GUI at: http://%s or http://neotick.local\n", wifiManager.getIP().c_str());
    }

    // Scroll "CONN" while waiting for WiFi
    if (!servicesStarted) {
        if (now - lastDisplayUpdate >= 300) {
            lastDisplayUpdate = now;
            ledDisplay.scrollCONN();
        }
        updateColonLED(now, true);
        return;
    }

    timeManager.update();
    webUI.update();

    // Colon LEDs: normal blink when connected, fast when WiFi lost
    updateColonLED(now, wifiManager.isWifiLost());

    // Animation from GUI
    // Buzzer test from GUI
    if (webUI.shouldTestBuzzer()) {
        playBuzzer();
    }

    if (webUI.shouldRunAnimation()) {
        webUI.setAnimating(true);
        ledDisplay.runCascadeAnimation();
        webUI.setAnimating(false);
        lastClockDisplay = -1;
    }

    DisplayMode mode = webUI.getMode();

    // Auto-return to clock after timer/tabata/pomodoro done (15 seconds of flashing)
    static unsigned long doneStartTime = 0;
    static bool buzzerPlayed = false;
    bool isDone = (mode == MODE_TIMER && webUI.isTimerDone()) || (mode == MODE_TABATA && webUI.isTabataDone()) || (mode == MODE_POMODORO && webUI.isPomodoroDone());
    if (isDone && doneStartTime == 0) {
        doneStartTime = now;
        buzzerPlayed = false;
    }
    if (isDone && !buzzerPlayed && settings.buzzerLevel) {
        buzzerPlayed = true;
        playBuzzer();
    }
    if (isDone && now - doneStartTime >= 3000) {
        if (mode == MODE_TIMER) webUI.timerReset();
        else if (mode == MODE_TABATA) webUI.tabataReset();
        else webUI.pomodoroReset();
        webUI.setMode(MODE_CLOCK);
        mode = MODE_CLOCK;
        doneStartTime = 0;
    }
    if (!isDone) doneStartTime = 0;

    // Helper: render number + apply color mode + show ONCE (no flicker)
    auto showAndMirror = [&](int val) {
        if (settings.colorMode >= 2 && !ledDisplay.hasOverrideColor()) {
            // For crazy/wave: render digits without showing, apply color, then show once
            // Skip when override is active (tabata/pomodoro own the color)
            ledDisplay.renderNumber(val);
            if (settings.colorMode == 2) ledDisplay.showCrazy();
            else if (settings.colorMode == 3) ledDisplay.showRainbowWave();
            ledDisplay.forceShow();
        } else {
            ledDisplay.showNumber(val);
        }
        webUI.setDisplayValue(val);
        webUI.setDisplayBlank(false);
    };
    auto showBlank = [&]() {
        ledDisplay.clear();
        webUI.setDisplayBlank(true);
    };

    // Check if anything is paused (running was stopped but not reset)
    bool isPaused = false;
    switch (mode) {
        case MODE_STOPWATCH: isPaused = !webUI.isStopwatchRunning() && webUI.getStopwatchElapsed() > 0; break;
        case MODE_TIMER:     isPaused = !webUI.isTimerRunning() && !webUI.isTimerDone() && webUI.getTimerRemaining() > 0 && webUI.getTimerRemaining() < (long)webUI.getTimerDuration(); break;
        case MODE_TABATA:    isPaused = !webUI.isTabataRunning() && !webUI.isTabataDone() && webUI.getTabataPhaseRemaining() > 0; break;
        case MODE_POMODORO:  isPaused = !webUI.isPomodoroRunning() && !webUI.isPomodoroDone() && webUI.getPomodoroPhaseRemaining() > 0; break;
        default: break;
    }

    // Apply color overrides BEFORE rendering
    if (settings.colorMode == 1) {
        static unsigned long lastRainbowUpdate = 0;
        static uint8_t rainbowHue = 0;
        if (now - lastRainbowUpdate >= 500) {
            lastRainbowUpdate = now;
            rainbowHue += 1;
        }
        ledDisplay.setOverrideColor(CHSV(rainbowHue, 255, 255));
    } else {
        ledDisplay.clearOverrideColor();
    }

    switch (mode) {
        case MODE_CLOCK: {
            if (now - lastDisplayUpdate >= CLOCK_REFRESH_MS) {
                lastDisplayUpdate = now;
                int display;
                if (settings.clockShowMMSS) {
                    display = timeManager.getMinutes() * 100 + timeManager.getSeconds();
                } else {
                    display = timeManager.get4Digit();
                }
                if (display != lastClockDisplay) {
                    if (settings.animateTransitions && settings.colorMode < 2) {
                        ledDisplay.showNumberFadeAnimated(display);
                        webUI.setDisplayValue(display);
                        webUI.setDisplayBlank(false);
                    } else {
                        showAndMirror(display);
                    }
                    lastClockDisplay = display;
                } else {
                    showAndMirror(display);
                }
            }
            break;
        }

        case MODE_STOPWATCH: {
            ledDisplay.clearOverrideColor();
            if (now - lastDisplayUpdate >= STOPWATCH_REFRESH_MS) {
                lastDisplayUpdate = now;
                unsigned long elapsed = webUI.getStopwatchElapsed();
                unsigned long totalSec = elapsed / 1000;
                int mins = totalSec / 60;
                int secs = totalSec % 60;
                showAndMirror((mins > 99) ? 9999 : (mins * 100 + secs));
            }
            if (isPaused) { ledDisplay.pulseBrightness(); }
            break;
        }

        case MODE_TIMER: {
            ledDisplay.clearOverrideColor();
            if (now - lastDisplayUpdate >= TIMER_REFRESH_MS) {
                lastDisplayUpdate = now;
                long remaining = webUI.getTimerRemaining();
                if (webUI.isTimerDone()) {
                    static bool tf = false; tf = !tf;
                    if (tf) showAndMirror(0); else showBlank();
                } else {
                    long s = (remaining + 999) / 1000;
                    int display = (s / 60 > 99) ? 9999 : ((int)(s / 60) * 100 + (int)(s % 60));
                    if (s <= 5 && settings.animateTransitions) {
                        ledDisplay.showNumberFadeAnimated(display, true);
                        webUI.setDisplayValue(display);
                        webUI.setDisplayBlank(false);
                    } else {
                        showAndMirror(display);
                    }
                }
            }
            if (isPaused) { ledDisplay.pulseBrightness(); }
            break;
        }

        case MODE_TABATA: {
            // Buzz on work/rest phase change
            if (settings.buzzerLevel && webUI.tabataPhaseChanged()) {
                playPhaseBeep(webUI.isTabataWorkPhase());
            }
            if (now - lastDisplayUpdate >= TABATA_REFRESH_MS) {
                lastDisplayUpdate = now;
                const TabataSettings& tb = webUI.getTabataSettings();
                if (webUI.isTabataDone()) {
                    static bool tbf = false; tbf = !tbf;
                    ledDisplay.clearOverrideColor();
                    if (tbf) showAndMirror(0); else showBlank();
                } else if (webUI.isTabataRunning()) {
                    if (webUI.isTabataWorkPhase())
                        ledDisplay.setOverrideColor(COLOR_TABLE[tb.workColorIdx % COLOR_COUNT].color);
                    else
                        ledDisplay.setOverrideColor(COLOR_TABLE[tb.restColorIdx % COLOR_COUNT].color);
                    long rem = webUI.getTabataPhaseRemaining();
                    long s = (rem + 999) / 1000;
                    int display = (s / 60 > 99) ? 9999 : ((int)(s / 60) * 100 + (int)(s % 60));
                    if (s <= 5 && settings.animateTransitions) {
                        ledDisplay.showNumberFadeAnimated(display, true);
                        webUI.setDisplayValue(display);
                        webUI.setDisplayBlank(false);
                    } else {
                        showAndMirror(display);
                    }
                } else {
                    ledDisplay.clearOverrideColor();
                    showAndMirror((tb.workSec / 60) * 100 + (tb.workSec % 60));
                    if (isPaused) { ledDisplay.pulseBrightness(); }
                }
            }
            break;
        }

        case MODE_POMODORO: {
            if (now - lastDisplayUpdate >= TABATA_REFRESH_MS) {
                lastDisplayUpdate = now;
                if (webUI.isPomodoroDone()) {
                    static bool pf = false; pf = !pf;
                    ledDisplay.clearOverrideColor();
                    if (pf) showAndMirror(0); else showBlank();
                } else if (webUI.isPomodoroRunning()) {
                    ledDisplay.setOverrideColor(webUI.isPomodoroWorkPhase() ? CRGB::Green : CRGB::Blue);
                    long rem = webUI.getPomodoroPhaseRemaining();
                    long s = (rem + 999) / 1000;
                    showAndMirror((s / 60 > 99) ? 9999 : ((int)(s / 60) * 100 + (int)(s % 60)));
                } else {
                    ledDisplay.clearOverrideColor();
                    showAndMirror((POMODORO_WORK_SEC / 60) * 100 + (POMODORO_WORK_SEC % 60));
                    if (isPaused) { ledDisplay.pulseBrightness(); }
                }
            }
            break;
        }
    }


    // Brightness state machine: night shift > normal
    {
        bool isNight = false;
        if (settings.nightShiftEnabled && timeManager.isTimeSynced()) {
            int h = timeManager.getHours();
            if (settings.nightShiftStartHour > settings.nightShiftEndHour)
                isNight = (h >= settings.nightShiftStartHour || h < settings.nightShiftEndHour);
            else
                isNight = (h >= settings.nightShiftStartHour && h < settings.nightShiftEndHour);
        }
        uint8_t targetBright;
        if (isNight) targetBright = settings.nightShiftBrightness;
        else targetBright = settings.brightness;

        ledDisplay.setBrightness(targetBright);
    }

    // Clockwork buzzer: chime on the hour (number of times = hour)
    if (settings.clockworkBuzzer && settings.buzzerLevel > 0 && mode == MODE_CLOCK && timeManager.isTimeSynced()) {
        static int lastChimeHour = -1;
        int h = timeManager.getHours();
        int m = timeManager.getMinutes();
        if (m == 0 && h != lastChimeHour) {
            // Check night shift — silence during night
            bool isNight = false;
            if (settings.nightShiftEnabled) {
                if (settings.nightShiftStartHour > settings.nightShiftEndHour)
                    isNight = (h >= settings.nightShiftStartHour || h < settings.nightShiftEndHour);
                else
                    isNight = (h >= settings.nightShiftStartHour && h < settings.nightShiftEndHour);
            }
            if (!isNight) {
                lastChimeHour = h;
                int chimes = h % 12;
                if (chimes == 0) chimes = 12;
                uint8_t v = buzzVol();
                for (int i = 0; i < chimes; i++) {
                    ledcWriteTone(0, 1200); ledcWrite(0, v); delay(120);
                    ledcWrite(0, 0); delay(180);
                }
            } else {
                lastChimeHour = h;  // skip but mark so we don't retry
            }
        }
        if (m != 0) lastChimeHour = -1;  // reset for next hour
    }

    // Date display (only in clock mode)
    if (mode == MODE_CLOCK && settings.showDateEnabled && timeManager.isTimeSynced()) {
        unsigned long dateIv = (unsigned long)settings.showDateIntervalSec * 1000UL;
        if (now - lastDateShow >= dateIv) {
            lastDateShow = now;
            int dd = timeManager.getDay();
            int mm = timeManager.getMonth();
            int dateDisp = dd * 100 + mm;
            ledDisplay.showNumber(dateDisp);
            digitalWrite(COLON_LED_PIN, HIGH);
            delay(2000);
            digitalWrite(COLON_LED_PIN, LOW);
            lastClockDisplay = -1;
        }
    }

    // Birthday check (once per hour, on the hour)
    if (mode == MODE_CLOCK && settings.birthdayCount > 0 && timeManager.isTimeSynced()) {
        int curH = timeManager.getHours();
        if (curH != lastBirthdayHour) {
            lastBirthdayHour = curH;
            int curDay = timeManager.getDay();
            int curMonth = timeManager.getMonth();
            for (int i = 0; i < settings.birthdayCount && i < MAX_BIRTHDAYS; i++) {
                Birthday b;
                configStore.loadBirthday(i, b);
                if (b.day == curDay && b.month == curMonth && b.name[0] != 0) {
                    char msg[64];
                    snprintf(msg, sizeof(msg), "HAPPY BDAY %s", b.name);
                    ledDisplay.scrollText(msg, 250);
                    // Brief celebration: flash colors
                    for (int j = 0; j < 10; j++) {
                        ledDisplay.setOverrideColor(CHSV(random(256), 255, 255));
                        ledDisplay.showNumber(0);
                        delay(100);
                    }
                    ledDisplay.clearOverrideColor();
                    lastClockDisplay = -1;
                }
            }
        }
    }
}
