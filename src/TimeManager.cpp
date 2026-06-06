#include "TimeManager.h"
#include <Arduino.h>
#include <sys/time.h>

const TimezoneEntry TimeManager::TIMEZONE_TABLE[] = {
    { "UTC-12:00 (Baker Island)",     -43200 },
    { "UTC-11:00 (Samoa)",            -39600 },
    { "UTC-10:00 (Hawaii)",           -36000 },
    { "UTC-09:00 (Alaska)",           -32400 },
    { "UTC-08:00 (Pacific US)",       -28800 },
    { "UTC-07:00 (Mountain US)",      -25200 },
    { "UTC-06:00 (Central US)",       -21600 },
    { "UTC-05:00 (Eastern US)",       -18000 },
    { "UTC-04:00 (Atlantic)",         -14400 },
    { "UTC-03:00 (Buenos Aires)",     -10800 },
    { "UTC-02:00 (Mid-Atlantic)",      -7200 },
    { "UTC-01:00 (Azores)",            -3600 },
    { "UTC+00:00 (London/GMT)",             0 },
    { "UTC+01:00 (Paris/Berlin)",       3600 },
    { "UTC+02:00 (Israel/Helsinki)",    7200 },
    { "UTC+03:00 (Moscow/Istanbul)",   10800 },
    { "UTC+03:30 (Tehran)",            12600 },
    { "UTC+04:00 (Dubai)",             14400 },
    { "UTC+05:00 (Karachi)",           18000 },
    { "UTC+05:30 (Mumbai)",            19800 },
    { "UTC+06:00 (Dhaka)",             21600 },
    { "UTC+07:00 (Bangkok)",           25200 },
    { "UTC+08:00 (Singapore/HK)",      28800 },
    { "UTC+09:00 (Tokyo)",             32400 },
    { "UTC+09:30 (Adelaide)",          34200 },
    { "UTC+10:00 (Sydney)",            36000 },
    { "UTC+11:00 (Solomon Is.)",       39600 },
    { "UTC+12:00 (Auckland)",          43200 },
};
const int TimeManager::TIMEZONE_COUNT = sizeof(TIMEZONE_TABLE) / sizeof(TIMEZONE_TABLE[0]);

void TimeManager::begin(WifiManager* wifiMgr) {
    m_wifiMgr = wifiMgr;
    m_ntpClient = new NTPClient(m_udp, NTP_SERVER, 0, 60000);
    m_ntpClient->begin();

    // Also start the ESP32 SNTP system clock (UTC). NTPClient only keeps whole seconds, which
    // leaves each watch's sub-second phase offset by up to ~1s; gettimeofday() gives ms-accurate
    // time aligned across devices, used by getEpochMillis() for animation/session sync.
    configTime(0, 0, NTP_SERVER);

    m_ntpClient->forceUpdate();
    if (m_ntpClient->isTimeSet()) {
        m_synced = true;
        checkDST();
        applyOffset();
        Serial.println("NTP initial sync OK");
    } else {
        Serial.println("NTP initial sync failed, will retry soon");
    }
    m_lastNtpSync = millis();
    m_lastDstCheck = millis();
}

void TimeManager::update() {
    unsigned long now = millis();

    // If not yet synced, retry every 5 seconds instead of waiting 30 min
    unsigned long syncInterval = m_synced ? NTP_SYNC_INTERVAL : 5000;

    if (now - m_lastNtpSync >= syncInterval) {
        m_lastNtpSync = now;
        if (m_ntpClient->forceUpdate()) {
            if (!m_synced) Serial.println("NTP sync OK");
            m_synced = true;
        }
    }

    if (!m_synced && m_ntpClient->isTimeSet()) {
        m_synced = true;
    }

    // Track the millis() at which each epoch second begins, so getEpochMillis() can
    // interpolate sub-second time aligned to the NTP-synced wall clock.
    if (m_synced) {
        unsigned long e = m_ntpClient->getEpochTime();
        if (e != m_epochAnchorSec) {
            m_epochAnchorSec = e;
            m_epochAnchorMillis = now;
        }
    }

    if (now - m_lastDstCheck >= DST_CHECK_INTERVAL) {
        m_lastDstCheck = now;
        checkDST();
    }
}

void TimeManager::checkDST() {
    bool wasDst = m_dstActive;
    switch (m_dstMode) {
        case 0: m_dstActive = false; break;
        case 1: m_dstActive = computeCustomDST(); break;
        case 2: m_dstActive = true; break;
    }
    if (wasDst != m_dstActive) {
        applyOffset();
        Serial.printf("DST changed: %s\n", m_dstActive ? "ON (summer)" : "OFF (winter)");
    }
}

void TimeManager::applyOffset() {
    long totalOffset = m_tzOffset + (m_dstActive ? m_dstOffset : 0);
    if (m_ntpClient) {
        m_ntpClient->setTimeOffset(totalOffset);
    }
}

int TimeManager::getHours() const   { return m_ntpClient ? m_ntpClient->getHours() : 0; }
int TimeManager::getMinutes() const { return m_ntpClient ? m_ntpClient->getMinutes() : 0; }
int TimeManager::getSeconds() const { return m_ntpClient ? m_ntpClient->getSeconds() : 0; }
int TimeManager::get4Digit() const  { return getHours() * 100 + getMinutes(); }
unsigned long TimeManager::getEpochTime() const { return m_ntpClient ? m_ntpClient->getEpochTime() : 0; }
uint64_t TimeManager::getEpochMillis() const {
    // Prefer the SNTP-disciplined system clock: ms-accurate and aligned across devices.
    struct timeval tv;
    if (gettimeofday(&tv, nullptr) == 0 && tv.tv_sec > 1600000000L) {  // set (post-2020)
        return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
    }
    // Fallback before SNTP has set the clock: whole-second epoch interpolated with millis().
    if (!m_synced) return 0;
    unsigned long frac = millis() - m_epochAnchorMillis;
    if (frac > 1000) frac = 1000;
    return (uint64_t)m_epochAnchorSec * 1000ULL + frac;
}
int TimeManager::getDay() const {
    if (!m_ntpClient) return 1;
    // getEpochTime() already includes timezone + DST offset via setTimeOffset
    time_t t = (time_t)m_ntpClient->getEpochTime();
    struct tm ti; gmtime_r(&t, &ti);
    return ti.tm_mday;
}
int TimeManager::getMonth() const {
    if (!m_ntpClient) return 1;
    time_t t = (time_t)m_ntpClient->getEpochTime();
    struct tm ti; gmtime_r(&t, &ti);
    return ti.tm_mon + 1;
}

void TimeManager::setTimezoneOffset(long offsetSeconds) {
    m_tzOffset = offsetSeconds;
    applyOffset();
}

void TimeManager::setDSTMode(int mode) {
    if (mode >= 0 && mode <= 2) {
        m_dstMode = mode;
        checkDST();
    }
}

void TimeManager::setDSTRules(const DSTRule& start, const DSTRule& end) {
    m_dstStart = start;
    m_dstEnd = end;
    checkDST();
}

void TimeManager::resetDSTToIsrael() {
    m_dstStart = DST_ISRAEL_START;
    m_dstEnd = DST_ISRAEL_END;
    checkDST();
}

int TimeManager::daysInMonth(int month) const {
    static const int days[] = { 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month >= 1 && month <= 12) return days[month];
    return 31;
}

// Generic: has the "first/last <dayOfWeek> of <month> at <hour>" passed?
bool TimeManager::hasRulePassed(const DSTRule& rule, int day, int weekDay, int hour, int month) const {
    if (month != rule.month) return false;

    int maxDay = daysInMonth(rule.month);

    if (rule.isLast) {
        // Last occurrence: must fall in the last 7 days of the month
        int earliestLast = maxDay - 6;  // e.g., for 31-day month: day 25-31

        if (day < earliestLast) return false;

        // How many days ago was the target day-of-week?
        int diff = (weekDay - rule.dayOfWeek + 7) % 7;  // days since last target day
        int targetDay = day - diff;  // the target day this week

        if (targetDay < earliestLast) {
            // Target day hasn't happened yet in the last-7-days window
            return false;
        }
        if (targetDay > day) return false;  // shouldn't happen

        if (day == targetDay) {
            return hour >= rule.hour;
        }
        return day > targetDay;  // past the target day
    } else {
        // First occurrence: must fall in the first 7 days
        if (day > 7) return true;  // definitely past it

        int diff = (weekDay - rule.dayOfWeek + 7) % 7;
        int targetDay = day - diff;

        if (targetDay < 1) return false;  // first occurrence hasn't happened yet
        if (day == targetDay) return hour >= rule.hour;
        return day > targetDay;
    }
}

bool TimeManager::computeCustomDST() const {
    if (!m_ntpClient || !m_synced) return false;

    unsigned long epoch = m_ntpClient->getEpochTime();
    time_t t = (time_t)epoch;
    // Get UTC time, then apply timezone for local date
    t += m_tzOffset;
    struct tm timeinfo;
    gmtime_r(&t, &timeinfo);

    int month   = timeinfo.tm_mon + 1;
    int day     = timeinfo.tm_mday;
    int weekDay = timeinfo.tm_wday;  // 0=Sunday
    int hour    = timeinfo.tm_hour;

    int startMonth = m_dstStart.month;
    int endMonth   = m_dstEnd.month;

    // Handle the case where DST spans across year boundary (e.g., Oct-Mar in southern hemisphere)
    bool normalOrder = (startMonth < endMonth);

    if (normalOrder) {
        // e.g., March -> October (northern hemisphere)
        if (month > startMonth && month < endMonth) return true;
        if (month < startMonth || month > endMonth) return false;
        if (month == startMonth) return hasRulePassed(m_dstStart, day, weekDay, hour, month);
        if (month == endMonth) return !hasRulePassed(m_dstEnd, day, weekDay, hour, month);
    } else {
        // e.g., October -> March (southern hemisphere)
        if (month > startMonth || month < endMonth) return true;
        if (month < startMonth && month > endMonth) return false;
        if (month == startMonth) return hasRulePassed(m_dstStart, day, weekDay, hour, month);
        if (month == endMonth) return !hasRulePassed(m_dstEnd, day, weekDay, hour, month);
    }

    return false;
}
