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
    }

    // Receive incoming messages
    receiveMessages();

    // Stuck-follower timeout: clear received session if host went silent
    if (m_receivedSession.active && !m_broadcasting) {
        if (millis() - m_receivedSession.lastReceivedMs > 3000)
            m_receivedSession.active = false;
    }

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
        doc["anc"] = m_broadcastState.anchorMs;
        doc["wc"] = m_broadcastState.workColorIdx;
        doc["rc"] = m_broadcastState.restColorIdx;
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
            if (memcmp(mac, m_myMac, 6) == 0) continue;

            // Update or add peer
            bool found = false;
            for (int i = 0; i < m_peerCount; i++) {
                if (memcmp(m_peers[i].mac, mac, 6) == 0) {
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
                m_receivedSession.anchorMs = doc["anc"] | (uint64_t)0;
                m_receivedSession.workColorIdx = doc["wc"] | 1;
                m_receivedSession.restColorIdx = doc["rc"] | 0;
                m_receivedSession.lastReceivedMs = millis();
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

String Heartbeat::sanitizeName(const String& in) {
    // DNS labels may only contain [a-z0-9-]; replace anything else with '-' and trim.
    String name = in;
    name.toLowerCase();
    String clean;
    for (size_t i = 0; i < name.length(); i++) {
        char c = name[i];
        clean += ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') ? c : '-';
    }
    while (clean.startsWith("-")) clean.remove(0, 1);
    while (clean.endsWith("-"))   clean.remove(clean.length() - 1);
    return clean;
}

String Heartbeat::getMdnsHost() const {
    // Build a unique, DNS-valid per-device hostname from the device name.
    String clean = sanitizeName(m_deviceName);
    if (clean.length() == 0) clean = "watch";  // fallback if name was all-invalid
    return clean.startsWith("neotick-") ? clean : ("neotick-" + clean);
}

bool Heartbeat::hasNameConflict() const {
    // Compare DNS-host form so "Home1" and "home1" are treated as the same.
    String mine = getMdnsHost();
    for (int i = 0; i < m_peerCount; i++) {
        String theirs = sanitizeName(m_peers[i].name);
        if (theirs.length() && !theirs.startsWith("neotick-")) theirs = "neotick-" + theirs;
        if (theirs == mine) return true;
    }
    return false;
}

void Heartbeat::registerMdns() {
    if (m_mdnsRegistered) {
        MDNS.end();
        m_mdnsRegistered = false;
    }

    String host = getMdnsHost();

    if (MDNS.begin(host.c_str())) {
        MDNS.addService("http", "tcp", 80);
        MDNS.addService(MDNS_SERVICE_NAME, "tcp", 80);
        m_mdnsRegistered = true;
        Serial.printf("[heartbeat] mDNS registered: %s.local\n", host.c_str());
    }
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
    // Store the DNS-safe form so the displayed name, mDNS host, and --list all match.
    String clean = sanitizeName(name);
    if (clean.length() == 0) return;  // ignore all-invalid input
    m_deviceName = clean;
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("devName", clean);
    prefs.end();
    // Re-register mDNS with the new name
    registerMdns();
}

// Host-side broadcast control
void Heartbeat::startBroadcast(uint8_t mode, bool running, long remainingMs,
                                bool workPhase, int interval, int totalIntervals, bool done,
                                uint8_t workColorIdx, uint8_t restColorIdx) {
    m_broadcasting = true;
    m_broadcastState.active = true;
    m_broadcastState.mode = mode;
    m_broadcastState.running = running;
    m_broadcastState.remainingMs = remainingMs;
    m_broadcastState.workPhase = workPhase;
    m_broadcastState.interval = interval;
    m_broadcastState.totalIntervals = totalIntervals;
    m_broadcastState.done = done;
    m_broadcastState.workColorIdx = workColorIdx;
    m_broadcastState.restColorIdx = restColorIdx;
    m_lastBroadcastSend = 0;  // send immediately
    Serial.printf("[heartbeat] broadcast started: mode=%d\n", mode);
}

void Heartbeat::updateBroadcast(bool running, long remainingMs,
                                 bool workPhase, int interval, bool done, uint64_t anchorMs) {
    if (!m_broadcasting) return;
    m_broadcastState.running = running;
    m_broadcastState.remainingMs = remainingMs;
    m_broadcastState.workPhase = workPhase;
    m_broadcastState.interval = interval;
    m_broadcastState.done = done;
    m_broadcastState.anchorMs = anchorMs;
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
