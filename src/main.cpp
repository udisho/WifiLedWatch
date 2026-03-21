#include <Arduino.h>
#include <ArduinoOTA.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
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

// Network-accessible log buffer
#define LOG_BUF_SIZE 2048
static char logBuf[LOG_BUF_SIZE];
static int logPos = 0;
const char* getLogBuffer() { return logBuf; }

// Custom Print class that tees to Serial + ring buffer
class LogTee : public Print {
public:
    size_t write(uint8_t c) override {
        Serial.write(c);
        logBuf[logPos] = (char)c;
        logPos = (logPos + 1) % (LOG_BUF_SIZE - 1);
        logBuf[logPos] = 0;
        return 1;
    }
    size_t write(const uint8_t* buf, size_t size) override {
        for (size_t i = 0; i < size; i++) write(buf[i]);
        return size;
    }
};
LogTee logger;

// Timing
unsigned long lastDisplayUpdate = 0;
unsigned long lastColonToggle = 0;
bool colonState = false;
int lastClockDisplay = -1;
unsigned long lastDateShow = 0;
unsigned long lastBirthdayCheck = 0;
int lastBirthdayHour = -1;

// Weather (fetched on Core 0 background task)
volatile float currentTemp = NAN;
volatile float currentFeelsLike = NAN;
char weatherCity[32] = {0};
bool showTempNext = false;

#define CLOCK_REFRESH_MS     200
#define STOPWATCH_REFRESH_MS  50
#define TABATA_REFRESH_MS     50
#define POMODORO_REFRESH_MS   50
#define TIMER_REFRESH_MS     100

// Night shift helper — shared by colon, brightness, clockwork, date display
bool isNightShiftActive() {
    if (!settings.nightShiftEnabled || !timeManager.isTimeSynced()) return false;
    int h = timeManager.getHours();
    if (settings.nightShiftStartHour > settings.nightShiftEndHour)
        return (h >= settings.nightShiftStartHour || h < settings.nightShiftEndHour);
    else
        return (h >= settings.nightShiftStartHour && h < settings.nightShiftEndHour);
}

// Colon LED helper
void updateColonLED(unsigned long now, bool wifiLost) {
    if (!settings.colonLedsEnabled || isNightShiftActive()) {
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

void buzzNote(int freq, int ms) {
    uint8_t v = buzzVol();
    ledcWriteTone(BUZZER_LEDC_CH, freq);
    ledcWrite(BUZZER_LEDC_CH, v);
    delay(ms);
    ledcWrite(BUZZER_LEDC_CH, 0);
}

// "Happy Birthday to You" — first phrase
void playHappyBirthday() {
    // C C D C F E | C C D C G F | ...
    // Using octave 5 frequencies
    const int C5=523, D5=587, E5=659, F5=698, G5=784, A5=880, Bb5=932, C6=1047;
    int melody[]  = { C5,C5, D5, C5, F5, E5,   C5,C5, D5, C5, G5, F5,   C5,C5, C6, A5, F5, E5, D5,   Bb5,Bb5, A5, F5, G5, F5 };
    int dur[]     = { 150,150, 300, 300, 300, 600,  150,150, 300, 300, 300, 600,  150,150, 300, 300, 300, 300, 600,  150,150, 300, 300, 300, 600 };
    int notes = sizeof(melody) / sizeof(melody[0]);
    for (int i = 0; i < notes; i++) {
        buzzNote(melody[i], dur[i]);
        delay(30);  // gap between notes
    }
}

// Cuckoo clock: high-low tone pair
void playCuckoo() {
    buzzNote(784, 180);  // G5
    delay(80);
    buzzNote(659, 280);  // E5
}

void playBuzzer() { playHappyBirthday(); }

void playPhaseBeep(bool isWork) {
    buzzNote(isWork ? 2000 : 800, 150);
}

// Check if today is any stored birthday
bool isBirthdayToday() {
    if (!timeManager.isTimeSynced() || settings.birthdayCount == 0) return false;
    int d = timeManager.getDay(), m = timeManager.getMonth();
    for (int i = 0; i < settings.birthdayCount && i < MAX_BIRTHDAYS; i++) {
        Birthday b; configStore.loadBirthday(i, b);
        if (b.day == d && b.month == m && b.name[0] != 0) return true;
    }
    return false;
}

// Weather background task — runs on Core 0, never blocks animations
void weatherTask(void* param) {
    float lat = NAN, lon = NAN;
    logger.printf("[weather] task started on core %d, free heap: %u\n", xPortGetCoreID(), ESP.getFreeHeap());

    for (;;) {
        if (!settings.showTempEnabled || !wifiManager.isConnected()) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        logger.printf("[weather] enabled, free heap: %u, stack HWM: %u\n", ESP.getFreeHeap(), uxTaskGetStackHighWaterMark(NULL));
        // Use stored coordinates if set by browser geolocation (re-check each cycle)
        if (settings.weatherLat != 0 && settings.weatherLon != 0) {
            lat = settings.weatherLat;
            lon = settings.weatherLon;
        }
        // Fall back to IP geolocation
        if (isnan(lat)) {
            logger.println("[weather] fetching location...");
            HTTPClient http;
            http.setTimeout(5000);
            http.begin("http://ip-api.com/json/?fields=lat,lon,city");
            int locCode = http.GET();
            logger.printf("[weather] ip-api response: %d\n", locCode);
            if (locCode == 200) {
                String body = http.getString();
                JsonDocument doc;
                if (!deserializeJson(doc, body)) {
                    lat = doc["lat"].as<float>();
                    lon = doc["lon"].as<float>();
                    const char* city = doc["city"].as<const char*>();
                    if (city) { strncpy(weatherCity, city, sizeof(weatherCity)-1); weatherCity[sizeof(weatherCity)-1] = 0; }
                    logger.printf("[weather] location: %.2f, %.2f (%s)\n", lat, lon, weatherCity);
                } else {
                    logger.println("[weather] JSON parse failed for location");
                }
            }
            http.end();
            if (isnan(lat)) {
                logger.println("[weather] no location, retry in 60s");
                vTaskDelay(pdMS_TO_TICKS(60000));
                continue;
            }
        }
        // Fetch temperature
        logger.println("[weather] fetching temp...");
        HTTPClient http;
        String url = "http://api.open-meteo.com/v1/forecast?latitude=";
        url += String(lat, 2); url += "&longitude="; url += String(lon, 2);
        url += "&current=temperature_2m,apparent_temperature";
        http.setTimeout(5000);
        http.begin(url);
        int code = http.GET();
        logger.printf("[weather] open-meteo response: %d, heap: %u\n", code, ESP.getFreeHeap());
        if (code == 200) {
            String body = http.getString();
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, body);
            if (!err) {
                float actual = doc["current"]["temperature_2m"].as<float>();
                float feels = doc["current"]["apparent_temperature"].as<float>();
                currentTemp = actual;
                currentFeelsLike = feels;
                float t = settings.tempFeelsLike ? feels : actual;
                logger.printf("[weather] actual=%.1f feels=%.1f, heap: %u\n", actual, feels, ESP.getFreeHeap());
            } else {
                logger.printf("[weather] JSON parse failed: %s\n", err.c_str());
            }
        } else {
            logger.printf("[weather] fetch failed: %d\n", code);
        }
        http.end();
        logger.printf("[weather] done, sleeping %ds. Heap: %u\n", WEATHER_FETCH_INTERVAL/1000, ESP.getFreeHeap());
        vTaskDelay(pdMS_TO_TICKS(WEATHER_FETCH_INTERVAL));
    }
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
    ledcSetup(BUZZER_LEDC_CH, 1000, 8);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);
    ledcWrite(BUZZER_LEDC_CH, 0);  // silence immediately
    if (settings.colorIndex >= 0) ledDisplay.setColorByIndex(settings.colorIndex);
    else ledDisplay.setColor(CRGB(settings.customR, settings.customG, settings.customB));

    ledDisplay.runStartupAnimation();
    ledDisplay.showCONN();
    wifiManager.begin();

    // Weather fetch on Core 0 (non-blocking)
    xTaskCreatePinnedToCore(weatherTask, "weather", 8192, NULL, 1, NULL, 0);

    Serial.printf("Setup complete (%lu ms)\n", millis());
}

void loop() {
    unsigned long now = millis();
    wifiManager.update();

    static bool dateShowing = false;
    static unsigned long dateShowStart = 0;
    static int dateDispVal = 0;
    static bool dateShowingTemp = false;
    static bool servicesStarted = false;
    if (wifiManager.isConnected() && !servicesStarted) {
        servicesStarted = true;

        // Start web server FIRST so GUI is available immediately
        timeManager.begin(&wifiManager);
        timeManager.setTimezoneOffset(settings.timezoneOffset);
        timeManager.setDSTMode(settings.dstMode);
        timeManager.setDSTRules(settings.dstStart, settings.dstEnd);
        webUI.begin(&ledDisplay, &timeManager, &configStore, &settings, &wifiManager);

        // ArduinoOTA for PlatformIO uploads
        ArduinoOTA.setHostname("neotick");
        ArduinoOTA.begin();

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

    ArduinoOTA.handle();
    timeManager.update();
    webUI.setCurrentTemp(currentTemp, currentFeelsLike);
    webUI.setWeatherCity(weatherCity);
    webUI.update();

    // Debug: print loop stats every 10s
    static unsigned long lastDebug = 0;
    static unsigned long loopCount = 0;
    loopCount++;
    if (now - lastDebug >= 10000) {
        logger.printf("[loop] %lu iter/10s, heap:%u, temp:%.1f, tempEn:%d\n",
            loopCount, ESP.getFreeHeap(), (float)currentTemp, settings.showTempEnabled);
        loopCount = 0;
        lastDebug = now;
    }

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
            else if (settings.colorMode == 3) ledDisplay.showPulse();
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
        if (now - lastRainbowUpdate >= 800) {
            lastRainbowUpdate = now;
            rainbowHue += 1;
        }
        ledDisplay.setOverrideColor(CHSV(rainbowHue, 255, 255));
    } else {
        ledDisplay.clearOverrideColor();
    }

    switch (mode) {
        case MODE_CLOCK: {
            if (dateShowing) break;  // date display owns the LEDs
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
            if (now - lastDisplayUpdate >= POMODORO_REFRESH_MS) {
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


    // Brightness state machine: thermal > night shift > normal
    {
        bool isNight = isNightShiftActive();
        uint8_t targetBright;
        if (isNight) targetBright = settings.nightShiftBrightness;
        else targetBright = settings.brightness;

        // Thermal protection: cap brightness after sustained high temp
        static unsigned long hotSince = 0;
        static bool thermalThrottled = false;
        int cpuTemp = (int)temperatureRead();
        if (cpuTemp >= THERMAL_THROTTLE_TEMP) {
            if (hotSince == 0) hotSince = now;
            if (now - hotSince >= 30000) thermalThrottled = true;  // hot for 30s
        } else {
            hotSince = 0;
            if (cpuTemp < THERMAL_THROTTLE_TEMP - 5) thermalThrottled = false;  // 5°C hysteresis
        }
        if (thermalThrottled && targetBright > THERMAL_THROTTLE_BRIGHT)
            targetBright = THERMAL_THROTTLE_BRIGHT;

        ledDisplay.setBrightness(targetBright);
    }

    // Clockwork buzzer: cuckoo on the hour, or Happy Birthday on birthdays
    if (settings.clockworkBuzzer && settings.buzzerLevel > 0 && mode == MODE_CLOCK && timeManager.isTimeSynced()) {
        static int lastChimeHour = -1;
        int h = timeManager.getHours();
        int m = timeManager.getMinutes();
        if (m == 0 && h != lastChimeHour) {
            if (!isNightShiftActive()) {
                lastChimeHour = h;
                if (isBirthdayToday()) {
                    playHappyBirthday();
                } else {
                    int chimes = h % 12;
                    if (chimes == 0) chimes = 12;
                    for (int i = 0; i < chimes; i++) {
                        playCuckoo();
                        delay(300);
                    }
                }
            } else {
                lastChimeHour = h;  // skip but mark so we don't retry
            }
        }
        if (m != 0) lastChimeHour = -1;  // reset for next hour
    }

    // Info display: date and/or temperature (only in clock mode) — non-blocking
    // Phase 0 = date (2s), phase 1 = temp (2s). If only one enabled, single phase.
    static int infoPhase = 0;  // 0=date, 1=temp
    bool infoEnabled = (settings.showDateEnabled || settings.showTempEnabled) && timeManager.isTimeSynced();
    if (mode == MODE_CLOCK && infoEnabled) {
        unsigned long infoIv = (unsigned long)settings.showDateIntervalSec * 1000UL;
        if (!dateShowing && now - lastDateShow >= infoIv) {
            dateShowing = true;
            dateShowStart = now;
            // Start with date if enabled, otherwise temp
            if (settings.showDateEnabled) {
                infoPhase = 0;
                int dd = timeManager.getDay(), mm = timeManager.getMonth();
                dateDispVal = dd * 100 + mm;
                dateShowingTemp = false;
                if (!isNightShiftActive() && settings.colonLedsEnabled)
                    digitalWrite(COLON_LED_PIN, HIGH);
            } else {
                infoPhase = 1;
                float t = settings.tempFeelsLike ? currentFeelsLike : currentTemp;
                if (isnan(t)) { dateShowing = false; lastDateShow = now - infoIv + 5000UL; }
                else { dateDispVal = (int)roundf(t); dateShowingTemp = true; digitalWrite(COLON_LED_PIN, LOW); }
            }
        }
        // Transition from date phase to temp phase after 2s
        if (dateShowing && infoPhase == 0 && now - dateShowStart >= 2000) {
            if (settings.showTempEnabled && !isnan((float)currentTemp)) {
                infoPhase = 1;
                dateShowStart = now;
                float t = settings.tempFeelsLike ? currentFeelsLike : currentTemp;
                dateDispVal = (int)roundf(t);
                dateShowingTemp = true;
                digitalWrite(COLON_LED_PIN, LOW);
            } else {
                // No temp, end cycle
                dateShowing = false; lastDateShow = now;
                webUI.setDisplayTemp(false); digitalWrite(COLON_LED_PIN, LOW);
                lastClockDisplay = -1;
            }
        }
        // End temp phase (or single phase) after 2s
        if (dateShowing && infoPhase == 1 && now - dateShowStart >= 2000) {
            dateShowing = false; lastDateShow = now;
            ledDisplay.clearOverrideColor();
            webUI.setDisplayTemp(false); digitalWrite(COLON_LED_PIN, LOW);
            lastClockDisplay = -1;
        }
        // Render current phase
        if (dateShowing) {
            if (dateShowingTemp) {
                if (settings.tempColorByValue) {
                    // Color by temperature: blue<5, cyan 5-15, green 15-22, orange 22-30, red>30
                    CRGB tc;
                    int t = dateDispVal;
                    if (t < 5)       tc = CRGB(0, 0, 255);
                    else if (t < 15) tc = CRGB(0, 200, 255);
                    else if (t < 22) tc = CRGB(0, 255, 0);
                    else if (t < 30) tc = CRGB(255, 140, 0);
                    else             tc = CRGB(255, 0, 0);
                    ledDisplay.setOverrideColor(tc);
                }
                ledDisplay.renderTemp(dateDispVal);
                ledDisplay.forceShow();
                webUI.setDisplayValue(dateDispVal);
                webUI.setDisplayBlank(false);
                webUI.setDisplayTemp(true);
            } else {
                webUI.setDisplayTemp(false);
                showAndMirror(dateDispVal);
            }
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
