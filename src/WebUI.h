#ifndef WEB_UI_H
#define WEB_UI_H

#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "Config.h"
#include "ConfigStore.h"

class LedDisplay;
class TimeManager;
class WifiManager;

class WebUI {
public:
    void begin(LedDisplay* display, TimeManager* timeMgr, ConfigStore* configStore, WatchSettings* settings, WifiManager* wifiMgr = nullptr);
    void update();

    DisplayMode getMode() const { return m_mode; }
    void setMode(DisplayMode mode) { m_mode = mode; }

    // Set what the physical display is currently showing (called by main.cpp)
    void setDisplayValue(int value) { m_displayValue = value; }
    void setDisplayBlank(bool blank) { m_displayBlank = blank; }

    // Animation trigger (consumed by main loop)
    bool shouldRunAnimation() { bool v = m_animationRequested; m_animationRequested = false; return v; }
    void setAnimating(bool v) { m_animating = v; }
    bool isAnimating() const { return m_animating; }

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

    // Animation
    bool m_animationRequested = false;
    bool m_animating = false;

    // Broadcast timing
    unsigned long m_lastBroadcast = 0;

    void setupRoutes();
    void handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len);
    void broadcastState();
    String buildStateJSON();
    void tabataAdvance();
    void pomodoroAdvance();
};

#endif // WEB_UI_H
