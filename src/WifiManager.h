#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <esp_wifi.h>
#include <ESPmDNS.h>
#include "Config.h"

// WiFi power modes
#define WIFI_POWER_SAVE   true
#define WIFI_POWER_FULL   false

enum WifiState {
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_AP_MODE,        // captive portal active
    WIFI_STATE_AP_GOT_CREDS,   // user submitted credentials
    WIFI_STATE_RECONNECTING
};

class WifiManager {
public:
    void begin();
    void update();  // call every loop iteration — non-blocking

    WifiState getState() const { return m_state; }
    bool isConnected() const { return m_state == WIFI_STATE_CONNECTED; }
    bool justConnected();  // returns true once after first connection
    String getIP() const;
    String getSSID() const { return m_ssid; }

    // Power management
    void setWifiPowerSave(bool enable);
    bool isWifiLost() const { return m_wifiWasConnected && m_state == WIFI_STATE_RECONNECTING; }

    // Credentials management (NVS)
    bool loadCredentials();
    void saveCredentials(const String& ssid, const String& password);
    void clearCredentials();

private:
    WifiState m_state = WIFI_STATE_IDLE;
    String m_ssid;
    String m_password;

    // Connection timing
    unsigned long m_connectStartTime = 0;
    unsigned long m_lastReconnectAttempt = 0;

    bool m_wifiWasConnected = false;
    bool m_justConnectedFlag = false;
    unsigned long m_apStartTime = 0;   // when AP mode started (for auto-shutdown)

    // Captive portal
    AsyncWebServer* m_apServer = nullptr;
    DNSServer m_dnsServer;
    bool m_apCredsReceived = false;
    String m_apReceivedSSID;
    String m_apReceivedPassword;

    void startCaptivePortal();
    void stopCaptivePortal();
    void startConnection();

};

#endif // WIFI_MANAGER_H
