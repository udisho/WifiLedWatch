#ifndef HEARTBEAT_H
#define HEARTBEAT_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESPmDNS.h>
#include "Config.h"

struct PeerInfo {
    uint8_t mac[6];
    String ip;
    String name;
    unsigned long lastSeen;
};

// Broadcast session state (sent by host, received by followers)
struct BroadcastSession {
    bool active = false;
    uint8_t mode;           // DisplayMode enum value
    bool running = false;
    long remainingMs = 0;
    bool workPhase = true;  // for tabata/pomodoro
    int interval = 1;       // current interval
    int totalIntervals = 1;
    bool done = false;
    unsigned long lastReceivedMs = 0;
    // Absolute NTP-epoch anchor (ms) for the running phase: for a countdown it's the epoch
    // time it reaches 0; for the stopwatch it's the epoch time it started. 0 = not anchored
    // (paused / host not NTP-synced). Lets every clock derive the same value frame-by-frame.
    uint64_t anchorMs = 0;
    // Tabata work/rest color indices into COLOR_TABLE, so followers match the host's config.
    uint8_t workColorIdx = 1;
    uint8_t restColorIdx = 0;
};

class Heartbeat {
public:
    void begin();
    void update();  // call every loop iteration

    // Peer info
    int getPeerCount() const { return m_peerCount; }
    const PeerInfo& getPeer(int i) const { return m_peers[i]; }

    String getMyMacStr() const;

    // Device name (optional, stored in NVS)
    const String& getDeviceName() const { return m_deviceName; }
    void setDeviceName(const String& name);

    // The per-device mDNS hostname (e.g. "neotick-gym1"). Single source of truth,
    // used for both the watch's own mDNS record and ArduinoOTA.
    String getMdnsHost() const;

    // true if another watch on the network reports the same name (mDNS collision risk).
    bool hasNameConflict() const;

    // true if this clock currently owns the well-known neotick.local entry point.
    bool isGateway() const { return m_isGateway; }

    // Reduce a user-entered name to a DNS-safe label: lowercase, only [a-z0-9-].
    static String sanitizeName(const String& in);

    // Broadcast session (host side)
    void startBroadcast(uint8_t mode, bool running, long remainingMs,
                        bool workPhase, int interval, int totalIntervals, bool done,
                        uint8_t workColorIdx = 1, uint8_t restColorIdx = 0);
    void updateBroadcast(bool running, long remainingMs,
                         bool workPhase, int interval, bool done, uint64_t anchorMs = 0);
    void stopBroadcast();
    bool isBroadcasting() const { return m_broadcasting; }

    // Broadcast session (receiver side)
    const BroadcastSession& getReceivedSession() const { return m_receivedSession; }
    bool hasActiveSession() const { return m_receivedSession.active; }

private:
    WiFiUDP m_udp;
    uint8_t m_myMac[6];
    PeerInfo m_peers[MAX_PEERS];
    int m_peerCount = 0;

    bool m_mdnsRegistered = false;
    unsigned long m_lastHeartbeat = 0;

    String m_deviceName;

    // Broadcast session (host)
    bool m_broadcasting = false;
    BroadcastSession m_broadcastState;
    unsigned long m_lastBroadcastSend = 0;

    // Broadcast session (receiver)
    BroadcastSession m_receivedSession;

    void sendHeartbeat();
    void receiveMessages();
    void evictStalePeers();
    void registerMdns();
    void loadDeviceName();
    void updateGateway();   // elect lowest-MAC clock to own neotick.local; claim/release it

    bool m_isGateway = false;
};

#endif // HEARTBEAT_H
