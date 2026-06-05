#ifndef WEB_UI_H
#define WEB_UI_H

#include <cmath>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "Config.h"
#include "ConfigStore.h"

class LedDisplay;
class TimeManager;
class WifiManager;
class Heartbeat;

class WebUI {
public:
    void begin(LedDisplay* display, TimeManager* timeMgr, ConfigStore* configStore, WatchSettings* settings, WifiManager* wifiMgr = nullptr, Heartbeat* heartbeat = nullptr);
    void update();

    DisplayMode getMode() const { return m_mode; }
    void setMode(DisplayMode mode) { m_mode = mode; }

    // True for a few seconds after a config sync (send or receive) — main loop shows "SYNC".
    bool syncFlashActive() const { return m_syncFlashUntil != 0 && (long)(millis() - m_syncFlashUntil) < 0; }

    // Set what the physical display is currently showing (called by main.cpp)
    void setDisplayValue(int value) { m_displayValue = value; }
    void setDisplayBlank(bool blank) { m_displayBlank = blank; }
    void setDisplayTemp(bool isTemp) { m_displayTemp = isTemp; }
    int getDisplayValue() const { return m_displayValue; }
    bool isDisplayBlank() const { return m_displayBlank; }

    // Animation trigger (consumed by main loop)
    bool shouldRunAnimation() { bool v = m_animationRequested; m_animationRequested = false; return v; }
    void setAnimating(bool v) { m_animating = v; }
    bool isAnimating() const { return m_animating; }

    // Buzzer test (consumed by main loop)
    bool shouldTestBuzzer() { bool v = m_buzzerTestRequested; m_buzzerTestRequested = false; return v; }

    // Weather temperature (set by main loop, read by state JSON)
    void setCurrentTemp(float actual, float feels) {
        bool wasNan = isnan(m_currentTemp);
        m_currentTemp = actual; m_currentFeelsLike = feels;
        if (wasNan && !isnan(actual) && m_ws && m_ws->count() > 0) m_ws->textAll(buildStateJSON());
    }
    void setWeatherCity(const char* city) { m_weatherCity = city ? city : ""; }

    // Stopwatch
    bool isStopwatchRunning() const { return m_swRunning; }
    unsigned long getStopwatchElapsed() const;
    void stopwatchStart();    // resume from accumulated
    void stopwatchRestart();  // reset and start fresh
    void stopwatchStop();
    void stopwatchReset();

    // Timer
    bool isTimerRunning() const { return m_timerRunning; }
    long getTimerRemaining() const;
    void timerSet(unsigned long durationMs);
    void timerStart();
    void timerStop();
    void timerReset();
    bool isTimerDone() const { return m_timerDone; }
    unsigned long getTimerDuration() const { return m_timerDuration; }

    // Tabata
    bool isTabataRunning() const { return m_tabRunning; }
    bool isTabataWorkPhase() const { return m_tabWorkPhase; }
    int  getTabataCurrentInterval() const { return m_tabCurrentInterval; }
    long getTabataPhaseRemaining() const;
    bool isTabataDone() const { return m_tabDone; }
    bool tabataPhaseChanged() { bool v = m_tabPhaseChanged; m_tabPhaseChanged = false; return v; }
    void tabataStart();
    void tabataStop();
    void tabataReset();

    // Access tabata settings for display color logic
    const TabataSettings& getTabataSettings() const { return m_settings->tabata; }

    // Pomodoro
    bool isPomodoroRunning() const { return m_pomRunning; }
    bool isPomodoroWorkPhase() const { return m_pomWorkPhase; }
    bool isPomodoroDone() const { return m_pomDone; }
    int  getPomodoroInterval() const { return m_pomCurrentInterval; }
    long getPomodoroPhaseRemaining() const;
    void pomodoroStart();
    void pomodoroStop();
    void pomodoroReset();

private:
    AsyncWebServer* m_server = nullptr;
    AsyncWebSocket* m_ws = nullptr;

    LedDisplay* m_display = nullptr;
    TimeManager* m_timeMgr = nullptr;
    ConfigStore* m_configStore = nullptr;
    WatchSettings* m_settings = nullptr;
    WifiManager* m_wifiMgr = nullptr;
    Heartbeat* m_heartbeat = nullptr;

    DisplayMode m_mode = MODE_CLOCK;

    // Stopwatch
    bool m_swRunning = false;
    unsigned long m_swStartTime = 0;
    unsigned long m_swAccumulated = 0;

    // Timer
    bool m_timerRunning = false;
    bool m_timerDone = false;
    unsigned long m_timerDuration = 60000;
    unsigned long m_timerStartTime = 0;
    unsigned long m_timerRemaining = 60000;

    // Tabata
    bool m_tabRunning = false;
    bool m_tabDone = false;
    bool m_tabWorkPhase = true;
    int  m_tabCurrentInterval = 1;
    unsigned long m_tabPhaseStart = 0;
    unsigned long m_tabPhaseDuration = 0;
    bool m_tabPhaseChanged = false;

    // Pomodoro
    bool m_pomRunning = false;
    bool m_pomDone = false;
    bool m_pomWorkPhase = true;
    int  m_pomCurrentInterval = 1;
    unsigned long m_pomPhaseStart = 0;
    unsigned long m_pomPhaseDuration = 0;

    // Display mirror
    int m_displayValue = 0;
    bool m_displayBlank = false;
    bool m_displayTemp = false;

    // Animation
    bool m_animationRequested = false;
    bool m_animating = false;
    bool m_buzzerTestRequested = false;
    float m_currentTemp = NAN;
    float m_currentFeelsLike = NAN;
    String m_weatherCity;

    // Deferred NVS save (avoid flooding on rapid changes)
    unsigned long m_pendingSave = 0;

    // Multi-watch config sync (set by WS cmd, executed from update() in main loop)
    bool m_syncRequested = false;
    // While now < this, the main loop shows "SYNC" on the digits (set on send & receive)
    unsigned long m_syncFlashUntil = 0;

    // Broadcast timing
    unsigned long m_lastBroadcast = 0;
    unsigned long m_lastFullBroadcast = 15000;  // stagger: offset from peer scan

    void setupRoutes();
    void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    void broadcastState();
    String buildFastJSON();
    String buildStateJSON();
    String buildConfigJSON();                 // syncable settings only (for push to peers)
    void applyConfigJSON(const String& body); // apply a received config push
    void doConfigSync();                      // push this watch's config to all peers
    void tabataAdvance();
    void pomodoroAdvance();
};

#endif // WEB_UI_H
