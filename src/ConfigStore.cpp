#include "ConfigStore.h"
#include <Preferences.h>

void ConfigStore::begin() {
    // Create NVS namespace if it doesn't exist (e.g., after full flash erase)
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.end();
}

void ConfigStore::load(WatchSettings& s) {
    Preferences prefs;
    prefs.begin(PREFS_NS, true);
    s.brightness     = prefs.getUChar("bright", DEFAULT_BRIGHTNESS);
    s.colorIndex     = prefs.getInt("colorIdx", 0);
    s.customR        = prefs.getUChar("customR", 0);
    s.customG        = prefs.getUChar("customG", 255);
    s.customB        = prefs.getUChar("customB", 0);
    s.timezoneOffset = prefs.getLong("tzOffset", 7200);
    s.dstMode        = prefs.getInt("dstMode", 1);

    // DST rules
    s.dstStart.isLast    = prefs.getBool("dsIsLast", true);
    s.dstStart.dayOfWeek = prefs.getInt("dsDow", 5);
    s.dstStart.month     = prefs.getInt("dsMon", 3);
    s.dstStart.hour      = prefs.getInt("dsHour", 2);
    s.dstEnd.isLast      = prefs.getBool("deIsLast", true);
    s.dstEnd.dayOfWeek   = prefs.getInt("deDow", 0);
    s.dstEnd.month       = prefs.getInt("deMon", 10);
    s.dstEnd.hour        = prefs.getInt("deHour", 2);

    // Tabata
    s.clockShowMMSS  = prefs.getBool("mmss", false);
    s.animateTransitions = prefs.getBool("animTr", true);

    s.nightShiftEnabled    = prefs.getBool("nsEn", false);
    s.nightShiftStartHour  = prefs.getUChar("nsStart", 22);
    s.nightShiftEndHour    = prefs.getUChar("nsEnd", 7);
    s.nightShiftBrightness = prefs.getUChar("nsBright", 15);

    s.tabata.workSec      = prefs.getUShort("tbWork", TABATA_DEFAULT_WORK_SEC);
    s.tabata.restSec      = prefs.getUShort("tbRest", TABATA_DEFAULT_REST_SEC);
    s.tabata.intervals    = prefs.getUChar("tbInt", TABATA_DEFAULT_INTERVALS);
    s.tabata.workColorIdx = prefs.getUChar("tbWC", 1);
    s.tabata.restColorIdx = prefs.getUChar("tbRC", 0);

    s.showDateEnabled     = prefs.getBool("dateEn", false);
    s.showDateIntervalSec = prefs.getUChar("dateInt", 30);
    s.colonLedsEnabled  = prefs.getBool("colonEn", true);
    s.buzzerLevel       = prefs.getUChar("buzzLv", 0);
    s.clockworkBuzzer   = prefs.getBool("cwBuzz", false);

    s.pomodoroIntervals   = prefs.getUChar("pomInt", POMODORO_INTERVALS);
    s.pomWorkSec          = prefs.getUShort("pomW", POMODORO_WORK_SEC);
    s.pomBreakSec         = prefs.getUShort("pomB", POMODORO_BREAK_SEC);
    s.colorMode           = prefs.getUChar("clrMode", 0);
    s.birthdayCount        = prefs.getUChar("bdCnt", 0);
    s.birthdayIntervalMins = prefs.getUChar("bdIntv", 60);
    s.birthdayScrollCount  = prefs.getUChar("bdScrl", 1);
    s.birthdayScrollSpeed  = prefs.getUChar("bdSpd", 3);
    s.birthdayBuzzer       = prefs.getBool("bdBuzz", true);

    s.showTempEnabled     = prefs.getBool("tempEn", false);
    s.tempFeelsLike       = prefs.getBool("tempFL", false);
    s.tempColorByValue    = prefs.getBool("tempClr", false);
    if (prefs.isKey("wLat")) s.weatherLat = prefs.getFloat("wLat", 0);
    if (prefs.isKey("wLon")) s.weatherLon = prefs.getFloat("wLon", 0);

    prefs.end();
}

void ConfigStore::save(const WatchSettings& s) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putUChar("bright", s.brightness);
    prefs.putInt("colorIdx", s.colorIndex);
    prefs.putUChar("customR", s.customR);
    prefs.putUChar("customG", s.customG);
    prefs.putUChar("customB", s.customB);
    prefs.putLong("tzOffset", s.timezoneOffset);
    prefs.putInt("dstMode", s.dstMode);

    prefs.putBool("dsIsLast", s.dstStart.isLast);
    prefs.putInt("dsDow", s.dstStart.dayOfWeek);
    prefs.putInt("dsMon", s.dstStart.month);
    prefs.putInt("dsHour", s.dstStart.hour);
    prefs.putBool("deIsLast", s.dstEnd.isLast);
    prefs.putInt("deDow", s.dstEnd.dayOfWeek);
    prefs.putInt("deMon", s.dstEnd.month);
    prefs.putInt("deHour", s.dstEnd.hour);

    prefs.putBool("mmss", s.clockShowMMSS);
    prefs.putBool("animTr", s.animateTransitions);

    prefs.putBool("nsEn", s.nightShiftEnabled);
    prefs.putUChar("nsStart", s.nightShiftStartHour);
    prefs.putUChar("nsEnd", s.nightShiftEndHour);
    prefs.putUChar("nsBright", s.nightShiftBrightness);

    prefs.putUShort("tbWork", s.tabata.workSec);
    prefs.putUShort("tbRest", s.tabata.restSec);
    prefs.putUChar("tbInt", s.tabata.intervals);
    prefs.putUChar("tbWC", s.tabata.workColorIdx);
    prefs.putUChar("tbRC", s.tabata.restColorIdx);

    prefs.putBool("dateEn", s.showDateEnabled);
    prefs.putUChar("dateInt", s.showDateIntervalSec);
    prefs.putBool("colonEn", s.colonLedsEnabled);
    prefs.putUChar("buzzLv", s.buzzerLevel);
    prefs.putBool("cwBuzz", s.clockworkBuzzer);

    prefs.putUChar("pomInt", s.pomodoroIntervals);
    prefs.putUShort("pomW", s.pomWorkSec);
    prefs.putUShort("pomB", s.pomBreakSec);
    prefs.putUChar("clrMode", s.colorMode);
    prefs.putUChar("bdCnt", s.birthdayCount);
    prefs.putUChar("bdIntv", s.birthdayIntervalMins);
    prefs.putUChar("bdScrl", s.birthdayScrollCount);
    prefs.putUChar("bdSpd", s.birthdayScrollSpeed);
    prefs.putBool("bdBuzz", s.birthdayBuzzer);

    prefs.putBool("tempEn", s.showTempEnabled);
    prefs.putBool("tempFL", s.tempFeelsLike);
    prefs.putBool("tempClr", s.tempColorByValue);
    prefs.putFloat("wLat", s.weatherLat);
    prefs.putFloat("wLon", s.weatherLon);

    prefs.end();
}

void ConfigStore::saveBrightness(uint8_t brightness) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putUChar("bright", brightness);
    prefs.end();
}

void ConfigStore::saveColor(int colorIndex, uint8_t r, uint8_t g, uint8_t b) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putInt("colorIdx", colorIndex);
    prefs.putUChar("customR", r);
    prefs.putUChar("customG", g);
    prefs.putUChar("customB", b);
    prefs.end();
}

void ConfigStore::saveTimezone(long offset) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putLong("tzOffset", offset);
    prefs.end();
}

void ConfigStore::saveDSTMode(int mode) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putInt("dstMode", mode);
    prefs.end();
}

void ConfigStore::saveDSTRules(const DSTRule& start, const DSTRule& end) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putBool("dsIsLast", start.isLast);
    prefs.putInt("dsDow", start.dayOfWeek);
    prefs.putInt("dsMon", start.month);
    prefs.putInt("dsHour", start.hour);
    prefs.putBool("deIsLast", end.isLast);
    prefs.putInt("deDow", end.dayOfWeek);
    prefs.putInt("deMon", end.month);
    prefs.putInt("deHour", end.hour);
    prefs.end();
}

void ConfigStore::saveTabata(const TabataSettings& tabata) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putUShort("tbWork", tabata.workSec);
    prefs.putUShort("tbRest", tabata.restSec);
    prefs.putUChar("tbInt", tabata.intervals);
    prefs.putUChar("tbWC", tabata.workColorIdx);
    prefs.putUChar("tbRC", tabata.restColorIdx);
    prefs.end();
}

void ConfigStore::saveNightShift(bool enabled, uint8_t startH, uint8_t endH, uint8_t bright) {
    Preferences prefs;
    prefs.begin(PREFS_NS, false);
    prefs.putBool("nsEn", enabled);
    prefs.putUChar("nsStart", startH);
    prefs.putUChar("nsEnd", endH);
    prefs.putUChar("nsBright", bright);
    prefs.end();
}

void ConfigStore::saveDateDisplay(bool enabled, uint8_t intervalSec) {
    Preferences prefs; prefs.begin(PREFS_NS, false);
    prefs.putBool("dateEn", enabled);
    prefs.putUChar("dateInt", intervalSec);
    prefs.end();
}

void ConfigStore::saveBuzzer(int level) {
    Preferences prefs; prefs.begin(PREFS_NS, false);
    prefs.putUChar("buzzLv", level);
    prefs.end();
}



void ConfigStore::savePomodoroIntervals(uint8_t intervals) {
    Preferences prefs; prefs.begin(PREFS_NS, false);
    prefs.putUChar("pomInt", intervals);
    prefs.end();
}

void ConfigStore::saveBirthday(int index, const Birthday& bday) {
    if (index < 0 || index >= MAX_BIRTHDAYS) return;
    Preferences prefs; prefs.begin(PREFS_NS, false);
    char k[10];
    snprintf(k, sizeof(k), "bday%dn", index); prefs.putString(k, bday.name);
    snprintf(k, sizeof(k), "bday%dd", index); prefs.putUChar(k, bday.day);
    snprintf(k, sizeof(k), "bday%dm", index); prefs.putUChar(k, bday.month);
    prefs.end();
}

void ConfigStore::loadBirthday(int index, Birthday& bday) {
    if (index < 0 || index >= MAX_BIRTHDAYS) return;
    Preferences prefs; prefs.begin(PREFS_NS, true);
    char k[10];
    snprintf(k, sizeof(k), "bday%dn", index); prefs.getString(k, bday.name, sizeof(bday.name));
    snprintf(k, sizeof(k), "bday%dd", index); bday.day = prefs.getUChar(k, 0);
    snprintf(k, sizeof(k), "bday%dm", index); bday.month = prefs.getUChar(k, 0);
    prefs.end();
}

void ConfigStore::saveTabataPreset(int index, const TabataPreset& preset) {
    if (index < 0 || index >= MAX_TABATA_PRESETS) return;
    Preferences prefs; prefs.begin(PREFS_NS, false);
    char k[10];
    snprintf(k, sizeof(k), "tp%dn", index); prefs.putString(k, preset.name);
    snprintf(k, sizeof(k), "tp%dw", index); prefs.putUShort(k, preset.workSec);
    snprintf(k, sizeof(k), "tp%dr", index); prefs.putUShort(k, preset.restSec);
    snprintf(k, sizeof(k), "tp%di", index); prefs.putUChar(k, preset.intervals);
    prefs.end();
}

void ConfigStore::loadTabataPreset(int index, TabataPreset& preset) {
    if (index < 0 || index >= MAX_TABATA_PRESETS) return;
    Preferences prefs; prefs.begin(PREFS_NS, true);
    char k[10];
    snprintf(k, sizeof(k), "tp%dn", index); if (prefs.isKey(k)) prefs.getString(k, preset.name, sizeof(preset.name));
    snprintf(k, sizeof(k), "tp%dw", index); if (prefs.isKey(k)) preset.workSec = prefs.getUShort(k, TABATA_DEFAULT_WORK_SEC);
    snprintf(k, sizeof(k), "tp%dr", index); if (prefs.isKey(k)) preset.restSec = prefs.getUShort(k, TABATA_DEFAULT_REST_SEC);
    snprintf(k, sizeof(k), "tp%di", index); if (prefs.isKey(k)) preset.intervals = prefs.getUChar(k, TABATA_DEFAULT_INTERVALS);
    prefs.end();
}

void ConfigStore::saveWeather(bool tempEnabled, bool feelsLike) {
    Preferences prefs; prefs.begin(PREFS_NS, false);
    prefs.putBool("tempEn", tempEnabled);
    prefs.putBool("tempFL", feelsLike);
    prefs.end();
}
