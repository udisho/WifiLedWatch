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
};

class Heartbeat {
public:
    void begin();
    void update();  // call every loop iteration

    // Peer info
    int getPeerCount() const { return m_peerCount; }
    const PeerInfo& getPeer(int i) const { return m_peers[i]; }

    // Election
    bool isMaster() const { return m_isMaster; }
    String getMyMacStr() const;

    // Device name (optional, stored in NVS)
    const String& getDeviceName() const { return m_deviceName; }
    void setDeviceName(const String& name);

    // Broadcast session (host side)
    void startBroadcast(uint8_t mode, bool running, long remainingMs,
                        bool workPhase, int interval, int totalIntervals, bool done);
    void updateBroadcast(bool running, long remainingMs,
                         bool workPhase, int interval, bool done);
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

    bool m_isMaster = false;
    bool m_mdnsRegistered = false;
    unsigned long m_lastHeartbeat = 0;
    unsigned long m_lastElection = 0;

    // Non-blocking election stagger
    bool m_electionPending = false;
    unsigned long m_electionStaggerStart = 0;
    unsigned long m_electionStaggerMs = 0;

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
    void runElection();
    void registerMdns();
    void unregisterMdns();
    int compareMac(const uint8_t* a, const uint8_t* b) const;
    void loadDeviceName();
};

#endif // HEARTBEAT_H
