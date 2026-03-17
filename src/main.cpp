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
unsigned long lastMessageScroll = 0;
unsigned long lastDateShow = 0;
unsigned long lastBirthdayCheck = 0;
int lastBirthdayHour = -1;

#define CLOCK_REFRESH_MS     200
#define STOPWATCH_REFRESH_MS  50
#define TABATA_REFRESH_MS     50
#define TIMER_REFRESH_MS     100

// Colon LED helper
void updateColonLED(unsigned long now, bool wifiLost) {
    unsigned long blinkRate = wifiLost ? COLON_BLINK_FAST_MS : COLON_BLINK_NORMAL_MS;
    if (now - lastColonToggle >= blinkRate) {
        lastColonToggle = now;
        colonState = !colonState;
        digitalWrite(COLON_LED_PIN, colonState ? HIGH : LOW);
    }
}

void playBuzzer() {
    ledcWriteTone(0, 1000); delay(200);
    ledcWriteTone(0, 0); delay(100);
    ledcWriteTone(0, 1500); delay(200);
    ledcWriteTone(0, 0); delay(100);
    ledcWriteTone(0, 2000); delay(400);
    ledcWriteTone(0, 0);
}

CRGB getSunriseColor(int hour) {
    if (hour >= 6 && hour < 8)   return CHSV(30, 255, 255);   // warm orange
    if (hour >= 8 && hour < 12)  return CHSV(0, 0, 255);      // bright white
    if (hour >= 12 && hour < 18) return CHSV(128, 80, 255);    // cool white/cyan
    if (hour >= 18 && hour < 21) return CHSV(35, 200, 255);    // warm amber
    return CHSV(160, 255, 80);                                  // dim blue
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
    ledcAttachPin(BUZZER_PIN, 0);
    ledcSetup(0, 2000, 8);
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

        Serial.printf("Access GUI at: http://%s or http://amazingwatch.local\n", wifiManager.getIP().c_str());
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
    if (isDone && !buzzerPlayed && settings.buzzerEnabled) {
        buzzerPlayed = true;
        playBuzzer();
    }
    if (isDone && now - doneStartTime >= 15000) {
        if (mode == MODE_TIMER) webUI.timerReset();
        else if (mode == MODE_TABATA) webUI.tabataReset();
        else webUI.pomodoroReset();
        webUI.setMode(MODE_CLOCK);
        mode = MODE_CLOCK;
        doneStartTime = 0;
    }
    if (!isDone) doneStartTime = 0;

    // Helper: show number on LEDs and mirror to GUI
    auto showAndMirror = [&](int val) {
        ledDisplay.showNumber(val);
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

    switch (mode) {
        case MODE_CLOCK: {
            if (settings.gymModeEnabled) {
                ledDisplay.setOverrideColor(CRGB::White);
            } else if (settings.sunriseColorEnabled) {
                ledDisplay.setOverrideColor(getSunriseColor(timeManager.getHours()));
            } else {
                ledDisplay.clearOverrideColor();
            }
            if (now - lastDisplayUpdate >= CLOCK_REFRESH_MS) {
                lastDisplayUpdate = now;
                int display;
                if (settings.clockShowMMSS) {
                    display = timeManager.getMinutes() * 100 + timeManager.getSeconds();
                } else {
                    display = timeManager.get4Digit();
                }
                if (display != lastClockDisplay) {
                    if (settings.animateTransitions) {
                        ledDisplay.showNumberFadeAnimated(display);
                    } else {
                        ledDisplay.showNumber(display);
                    }
                    webUI.setDisplayValue(display);
                    webUI.setDisplayBlank(false);
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
                    showAndMirror((s / 60 > 99) ? 9999 : ((int)(s / 60) * 100 + (int)(s % 60)));
                }
            }
            if (isPaused) { ledDisplay.pulseBrightness(); }
            break;
        }

        case MODE_CRAZY: {
            if (now - lastDisplayUpdate >= 150) {
                lastDisplayUpdate = now;
                // Each LED gets its own random color — show time digits but each LED is unique
                int display;
                if (settings.clockShowMMSS)
                    display = timeManager.getMinutes() * 100 + timeManager.getSeconds();
                else
                    display = timeManager.get4Digit();
                // First render the time digits normally (sets which LEDs are on)
                ledDisplay.clearOverrideColor();
                ledDisplay.showNumber(display);
                // Then colorize each lit LED individually with random colors
                ledDisplay.showCrazy();
                webUI.setDisplayValue(display);
                webUI.setDisplayBlank(false);
            }
            break;
        }

        case MODE_TABATA: {
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
                    showAndMirror((s / 60 > 99) ? 9999 : ((int)(s / 60) * 100 + (int)(s % 60)));
                } else {
                    ledDisplay.clearOverrideColor();
                    showAndMirror((tb.workSec / 60) * 100 + (tb.workSec % 60));
                    if (isPaused) { ledDisplay.pulseBrightness(); }
                }
            }
            break;
        }

        case MODE_RAINBOW: {
            if (now - lastDisplayUpdate >= 150) {
                lastDisplayUpdate = now;
                static uint8_t rainbowHue = 0;
                ledDisplay.setOverrideColor(CHSV(rainbowHue, 255, 255));
                rainbowHue += 1;
                int display;
                if (settings.clockShowMMSS)
                    display = timeManager.getMinutes() * 100 + timeManager.getSeconds();
                else
                    display = timeManager.get4Digit();
                showAndMirror(display);
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

    // Night shift: auto-adjust brightness based on time
    if (settings.nightShiftEnabled && !settings.gymModeEnabled) {
        int h = timeManager.getHours();
        bool isNight;
        if (settings.nightShiftStartHour > settings.nightShiftEndHour) {
            isNight = (h >= settings.nightShiftStartHour || h < settings.nightShiftEndHour);
        } else {
            isNight = (h >= settings.nightShiftStartHour && h < settings.nightShiftEndHour);
        }
        static bool wasNight = false;
        if (isNight != wasNight) {
            wasNight = isNight;
            ledDisplay.setBrightness(isNight ? settings.nightShiftBrightness : settings.brightness);
        }
    }

    // Gym mode: override brightness
    if (settings.gymModeEnabled) {
        ledDisplay.setBrightness(MAX_BRIGHTNESS);
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

    // Custom scrolling message
    if (mode == MODE_CLOCK && settings.messageEnabled && settings.customMessage[0] != 0) {
        unsigned long msgIv = (unsigned long)settings.messageIntervalMin * 60000UL;
        if (msgIv > 0 && now - lastMessageScroll >= msgIv) {
            lastMessageScroll = now;
            ledDisplay.scrollText(settings.customMessage, 300);
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
