#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include <WiFiUdp.h>
#include <NTPClient.h>
#include "Config.h"

class WifiManager;  // forward declaration for power management

struct TimezoneEntry {
    const char* label;
    long offsetSeconds;
};

// DST mode: 0=off, 1=custom rules (Israel default), 2=always on
class TimeManager {
public:
    void begin(WifiManager* wifiMgr = nullptr);
    void update();

    int getHours() const;
    int getMinutes() const;
    int getSeconds() const;
    int get4Digit() const;
    int getDay() const;
    int getMonth() const;

    // Timezone
    void setTimezoneOffset(long offsetSeconds);
    long getTimezoneOffset() const { return m_tzOffset; }

    // DST
    void setDSTMode(int mode);
    int  getDSTMode() const { return m_dstMode; }
    bool isDSTActive() const { return m_dstActive; }

    // Custom DST rules
    void setDSTRules(const DSTRule& start, const DSTRule& end);
    void resetDSTToIsrael();
    const DSTRule& getDSTStart() const { return m_dstStart; }
    const DSTRule& getDSTEnd() const { return m_dstEnd; }

    unsigned long getEpochTime() const;
    // Sub-second NTP-synced time (ms since epoch). Identical across NTP-synced clocks,
    // so it can phase-lock animations/rotations without any device-to-device messaging.
    // Returns 0 if not yet synced.
    uint64_t getEpochMillis() const;
    bool isTimeSynced() const { return m_synced; }

    static const TimezoneEntry TIMEZONE_TABLE[];
    static const int TIMEZONE_COUNT;

private:
    WiFiUDP m_udp;
    NTPClient* m_ntpClient = nullptr;
    WifiManager* m_wifiMgr = nullptr;

    long m_tzOffset = 7200;
    long m_dstOffset = 3600;
    int  m_dstMode = 1;          // 0=off, 1=custom rules, 2=always on
    bool m_dstActive = false;
    bool m_synced = false;

    DSTRule m_dstStart = DST_ISRAEL_START;
    DSTRule m_dstEnd   = DST_ISRAEL_END;

    unsigned long m_lastNtpSync = 0;
    unsigned long m_lastDstCheck = 0;

    // Anchor for sub-second epoch interpolation (see getEpochMillis)
    unsigned long m_epochAnchorSec = 0;
    unsigned long m_epochAnchorMillis = 0;

    void checkDST();
    bool computeCustomDST() const;
    bool hasRulePassed(const DSTRule& rule, int day, int weekDay, int hour, int month) const;
    int daysInMonth(int month) const;
    void applyOffset();
};

#endif // TIME_MANAGER_H
