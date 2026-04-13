#include "Heartbeat.h"
#include <ArduinoJson.h>
#include <Preferences.h>

static const IPAddress MULTICAST_ADDR(239, 1, 2, 3);

void Heartbeat::begin() {
    WiFi.macAddress(m_myMac);
    loadDeviceName();

    m_udp.beginMulticast(MULTICAST_ADDR, MULTICAST_PORT);
    Serial.printf("[heartbeat] started, MAC: %s, name: %s\n",
                  getMyMacStr().c_str(), m_deviceName.c_str());

    // First election immediately
    m_isMaster = true;  // assume master until proven otherwise
    registerMdns();
}

void Heartbeat::update() {
    if (WiFi.status() != WL_CONNECTED) return;

    unsigned long now = millis();

    // Send heartbeat
    if (now - m_lastHeartbeat >= HEARTBEAT_INTERVAL) {
        m_lastHeartbeat = now;
        sendHeartbeat();
        evictStalePeers();
        runElection();
    }

    // Receive incoming messages
    receiveMessages();

    // Send broadcast session state (host, every 200ms)
    if (m_broadcasting && now - m_lastBroadcastSend >= 200) {
        m_lastBroadcastSend = now;
        JsonDocument doc;
        doc["type"] = "bs";
        doc["mode"] = m_broadcastState.mode;
        doc["run"] = m_broadcastState.running;
        doc["ms"] = m_broadcastState.remainingMs;
        doc["work"] = m_broadcastState.workPhase;
        doc["int"] = m_broadcastState.interval;
        doc["total"] = m_broadcastState.totalIntervals;
        doc["done"] = m_broadcastState.done;
        char buf[200];
        serializeJson(doc, buf, sizeof(buf));
        m_udp.beginMulticastPacket();
        m_udp.print(buf);
        m_udp.endPacket();
    }
}

void Heartbeat::sendHeartbeat() {
    JsonDocument doc;
    doc["type"] = "hb";
    doc["mac"] = getMyMacStr();
    doc["ip"] = WiFi.localIP().toString();
    doc["name"] = m_deviceName;
    doc["up"] = millis() / 1000;
    doc["master"] = m_isMaster;

    char buf[200];
    serializeJson(doc, buf, sizeof(buf));

    m_udp.beginMulticastPacket();
    m_udp.print(buf);
    m_udp.endPacket();
}

void Heartbeat::receiveMessages() {
    while (m_udp.parsePacket()) {
        char buf[300];
        int len = m_udp.read(buf, sizeof(buf) - 1);
        if (len <= 0) continue;
        buf[len] = 0;

        JsonDocument doc;
        if (deserializeJson(doc, buf)) continue;

        const char* type = doc["type"];
        if (!type) continue;

        if (strcmp(type, "hb") == 0) {
            // Heartbeat from another watch
            const char* macStr = doc["mac"];
            const char* ip = doc["ip"];
            const char* name = doc["name"];
            if (!macStr || !ip) continue;

            // Parse MAC
            uint8_t mac[6];
            sscanf(macStr, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
                   &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]);

            // Skip self
            if (compareMac(mac, m_myMac) == 0) continue;

            // Update or add peer
            bool found = false;
            for (int i = 0; i < m_peerCount; i++) {
                if (compareMac(m_peers[i].mac, mac) == 0) {
                    m_peers[i].ip = ip;
                    m_peers[i].name = name ? name : "";
                    m_peers[i].lastSeen = millis();
                    found = true;
                    break;
                }
            }
            if (!found && m_peerCount < MAX_PEERS) {
                memcpy(m_peers[m_peerCount].mac, mac, 6);
                m_peers[m_peerCount].ip = ip;
                m_peers[m_peerCount].name = name ? name : "";
                m_peers[m_peerCount].lastSeen = millis();
                m_peerCount++;
                Serial.printf("[heartbeat] new peer: %s (%s)\n", ip, name ? name : "?");
                // Re-run election when new peer appears
                runElection();
            }
        }
        else if (strcmp(type, "bs") == 0) {
            // Broadcast session from host — only process if we're NOT the host
            if (!m_broadcasting) {
                m_receivedSession.active = true;
                m_receivedSession.mode = doc["mode"] | 0;
                m_receivedSession.running = doc["run"] | false;
                m_receivedSession.remainingMs = doc["ms"] | 0L;
                m_receivedSession.workPhase = doc["work"] | true;
                m_receivedSession.interval = doc["int"] | 1;
                m_receivedSession.totalIntervals = doc["total"] | 1;
                m_receivedSession.done = doc["done"] | false;
            }
        }
        else if (strcmp(type, "bs_stop") == 0) {
            if (!m_broadcasting) {
                m_receivedSession.active = false;
            }
        }
    }
}

void Heartbeat::evictStalePeers() {
    unsigned long now = millis();
    for (int i = m_peerCount - 1; i >= 0; i--) {
        if (now - m_peers[i].lastSeen > PEER_TIMEOUT) {
            Serial.printf("[heartbeat] peer lost: %s\n", m_peers[i].ip.c_str());
            // Shift remaining peers down
            for (int j = i; j < m_peerCount - 1; j++) {
                m_peers[j] = m_peers[j + 1];
            }
            m_peerCount--;
        }
    }
}

void Heartbeat::runElection() {
    // Non-blocking stagger: if waiting, check if delay elapsed
    if (m_electionPending) {
        if (millis() - m_electionStaggerStart < m_electionStaggerMs) return;
        m_electionPending = false;
        // Re-check after stagger — did someone else claim master?
        receiveMessages();
        bool stillBest = true;
        for (int i = 0; i < m_peerCount; i++) {
            if (compareMac(m_peers[i].mac, m_myMac) < 0) {
                stillBest = false;
                break;
            }
        }
        if (stillBest && !m_isMaster) {
            m_isMaster = true;
            Serial.println("[heartbeat] role: MASTER");
            registerMdns();
        }
        return;
    }

    // Lowest MAC wins master role
    bool shouldBeMaster = true;
    for (int i = 0; i < m_peerCount; i++) {
        if (compareMac(m_peers[i].mac, m_myMac) < 0) {
            shouldBeMaster = false;
            break;
        }
    }

    if (shouldBeMaster && !m_isMaster) {
        // Start non-blocking stagger to prevent split-brain
        m_electionPending = true;
        m_electionStaggerStart = millis();
        m_electionStaggerMs = (m_myMac[5] % 10) * ELECTION_STAGGER_MS;
        return;
    }

    if (shouldBeMaster != m_isMaster) {
        m_isMaster = shouldBeMaster;
        Serial.printf("[heartbeat] role: %s\n", m_isMaster ? "MASTER" : "follower");
        if (m_isMaster) {
            registerMdns();
        } else {
            unregisterMdns();
        }
    }
}

void Heartbeat::registerMdns() {
    if (m_mdnsRegistered) {
        MDNS.end();
        m_mdnsRegistered = false;
    }
    if (MDNS.begin("neotick")) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService(MDNS_SERVICE_NAME, "tcp", 80);
        m_mdnsRegistered = true;
        Serial.println("[heartbeat] mDNS registered: neotick.local");
    }
}

void Heartbeat::unregisterMdns() {
    if (m_mdnsRegistered) {
        MDNS.end();
        m_mdnsRegistered = false;
        Serial.println("[heartbeat] mDNS unregistered");
    }
}

int Heartbeat::compareMac(const uint8_t* a, const uint8_t* b) const {
    return memcmp(a, b, 6);
}

String Heartbeat::getMyMacStr() const {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             m_myMac[0], m_myMac[1], m_myMac[2], m_myMac[3], m_myMac[4], m_myMac[5]);
    return String(buf);
}

void Heartbeat::loadDeviceName() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    m_deviceName = prefs.getString("devName", "");
    prefs.end();
    if (m_deviceName.length() == 0) {
        // Default name from last 4 hex chars of MAC
        char buf[12];
        snprintf(buf, sizeof(buf), "NeoTick-%02X%02X", m_myMac[4], m_myMac[5]);
        m_deviceName = buf;
    }
}

void Heartbeat::setDeviceName(const String& name) {
    m_deviceName = name;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("devName", name);
    prefs.end();
}

// Host-side broadcast control
void Heartbeat::startBroadcast(uint8_t mode, bool running, long remainingMs,
                                bool workPhase, int interval, int totalIntervals, bool done) {
    m_broadcasting = true;
    m_broadcastState.active = true;
    m_broadcastState.mode = mode;
    m_broadcastState.running = running;
    m_broadcastState.remainingMs = remainingMs;
    m_broadcastState.workPhase = workPhase;
    m_broadcastState.interval = interval;
    m_broadcastState.totalIntervals = totalIntervals;
    m_broadcastState.done = done;
    m_lastBroadcastSend = 0;  // send immediately
    Serial.printf("[heartbeat] broadcast started: mode=%d\n", mode);
}

void Heartbeat::updateBroadcast(bool running, long remainingMs,
                                 bool workPhase, int interval, bool done) {
    if (!m_broadcasting) return;
    m_broadcastState.running = running;
    m_broadcastState.remainingMs = remainingMs;
    m_broadcastState.workPhase = workPhase;
    m_broadcastState.interval = interval;
    m_broadcastState.done = done;
}

void Heartbeat::stopBroadcast() {
    if (!m_broadcasting) return;
    m_broadcasting = false;
    m_broadcastState.active = false;

    // Send stop message
    JsonDocument doc;
    doc["type"] = "bs_stop";
    char buf[32];
    serializeJson(doc, buf, sizeof(buf));
    m_udp.beginMulticastPacket();
    m_udp.print(buf);
    m_udp.endPacket();
    Serial.println("[heartbeat] broadcast stopped");
}
