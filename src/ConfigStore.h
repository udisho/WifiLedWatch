#ifndef CONFIG_STORE_H
#define CONFIG_STORE_H

#include <Arduino.h>
#include "Config.h"

#define PREFS_NS "watchsettings"

struct TabataSettings {
    uint16_t workSec = TABATA_DEFAULT_WORK_SEC;
    uint16_t restSec = TABATA_DEFAULT_REST_SEC;
    uint8_t  intervals = TABATA_DEFAULT_INTERVALS;
    uint8_t  workColorIdx = 1;   // Green
    uint8_t  restColorIdx = 0;   // Red
};

struct Birthday {
    char name[16] = {0};
    uint8_t day = 0;
    uint8_t month = 0;
};

struct TabataPreset {
    char name[16] = {0};
    uint16_t workSec = 20;
    uint16_t restSec = 10;
    uint8_t  intervals = 8;
};

struct WatchSettings {
    uint8_t brightness = DEFAULT_BRIGHTNESS;
    int colorIndex = 0;
    uint8_t customR = 0, customG = 255, customB = 0;
    long timezoneOffset = 7200;      // Israel UTC+2
    int dstMode = 1;                  // 0=off, 1=custom rules, 2=always on
    DSTRule dstStart = DST_ISRAEL_START;
    DSTRule dstEnd   = DST_ISRAEL_END;
    bool clockShowMMSS = false;       // false=HH:MM, true=MM:SS
    bool animateTransitions = true;   // fade animation on digit change
    bool nightShiftEnabled = false;
    uint8_t nightShiftStartHour = 22;  // 10 PM
    uint8_t nightShiftEndHour = 7;      // 7 AM
    uint8_t nightShiftBrightness = 15;  // very dim at night
    TabataSettings tabata;
    // Date display
    bool showDateEnabled = false;
    uint8_t showDateIntervalSec = 30;
    // Colon LEDs
    bool colonLedsEnabled = true;
    // Buzzer: 0=off, 1=low, 2=high
    uint8_t buzzerLevel = 2;
    bool clockworkBuzzer = false;  // chime on the hour

    // Pomodoro intervals
    uint8_t pomodoroIntervals = POMODORO_INTERVALS;
    // Color mode: 0=static, 1=rainbow, 2=crazy, 3=pulse
    uint8_t colorMode = 0;
    // Birthday count (actual data in NVS)
    uint8_t birthdayCount = 0;
};

class ConfigStore {
public:
    void begin();
    void load(WatchSettings& settings);
    void save(const WatchSettings& settings);

    void saveBrightness(uint8_t brightness);
    void saveColor(int colorIndex, uint8_t r, uint8_t g, uint8_t b);
    void saveTimezone(long offset);
    void saveDSTMode(int mode);
    void saveDSTRules(const DSTRule& start, const DSTRule& end);
    void saveTabata(const TabataSettings& tabata);
    void saveNightShift(bool enabled, uint8_t startH, uint8_t endH, uint8_t bright);
    void saveDateDisplay(bool enabled, uint8_t intervalSec);
    void saveBuzzer(int level);

    void savePomodoroIntervals(uint8_t intervals);
    void saveBirthday(int index, const Birthday& bday);
    void loadBirthday(int index, Birthday& bday);
    void saveTabataPreset(int index, const TabataPreset& preset);
    void loadTabataPreset(int index, TabataPreset& preset);
};

#endif // CONFIG_STORE_H
