#include "WifiManager.h"
#include <Preferences.h>
#include <Arduino.h>

// Captive portal HTML — matching main GUI design
static const char AP_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="he">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<title>The Amazing Watch</title>
<style>
:root{--bg:#0f0f23;--card:#1a1a2e;--accent:#44d9e1;--accent2:#6e7dff;--text:#e0e0e0;--text2:#999;--btn:#2d2d44}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
.hdr{background:linear-gradient(135deg,var(--accent2),var(--accent));padding:24px 16px;text-align:center}
.hdr h1{font-size:26px;color:#fff;font-weight:800}
.hdr .sub{font-size:12px;color:rgba(255,255,255,.7);margin-top:4px}
.wrap{max-width:440px;margin:0 auto;padding:16px}
.card{background:var(--card);border-radius:14px;padding:22px;margin-bottom:16px}
.card h2{font-size:18px;color:var(--accent);margin-bottom:6px;font-weight:700}
.card p{font-size:14px;color:var(--text2);margin-bottom:16px;line-height:1.5}
.lang-bar{display:flex;justify-content:center;gap:8px;margin:14px 0}
.lang-btn{padding:8px 14px;border-radius:8px;border:2px solid #333;background:var(--btn);color:var(--text);font-size:13px;font-weight:600;cursor:pointer;transition:.15s}
.lang-btn.active{border-color:var(--accent);color:var(--accent)}
label{font-size:14px;color:var(--text2);display:block;margin-top:14px;margin-bottom:4px}
input[type=text],input[type=password]{width:100%;padding:14px;border-radius:10px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:16px}
input:focus{border-color:var(--accent);outline:none}
.btn{width:100%;padding:16px;border:none;border-radius:12px;font-size:17px;font-weight:700;cursor:pointer;margin-top:18px;transition:.15s;touch-action:manipulation}
.btn:active{transform:scale(.97)}
.btn-primary{background:var(--accent);color:#000}
.btn-help{background:var(--btn);color:var(--text);margin-top:10px;font-size:14px}
.help-box{display:none;background:rgba(255,255,255,.04);border-radius:10px;padding:16px;margin-top:14px;border:1px solid #222}
.help-box.show{display:block}
.card p{font-size:13px;color:var(--text2);margin:8px 0;line-height:1.6}
[data-lang]{display:none}[data-lang].show{display:block}
.foot{text-align:center;padding:20px 16px;font-size:12px;color:#555}
.foot a{color:var(--accent);text-decoration:none;font-weight:600}
.foot a:hover{text-decoration:underline}
.ig{display:inline-flex;align-items:center;gap:4px}
</style>
</head>
<body>
<div class="hdr">
  <h1>The Amazing Watch</h1>
  <div class="sub">By Udi & Noam Shorer</div>
</div>
<div class="wrap">
  <div class="lang-bar">
    <button class="lang-btn active" onclick="setLang('he')">HE</button>
    <button class="lang-btn" onclick="setLang('en')">EN</button>
    <button class="lang-btn" onclick="setLang('ru')">RU</button>
  </div>

  <div class="card">
    <h2 data-lang="he" class="show" style="direction:rtl;text-align:right">&#x05D4;&#x05D2;&#x05D3;&#x05E8;&#x05EA; &#x05E8;&#x05E9;&#x05EA; Wi-Fi</h2>
    <h2 data-lang="en">Wi-Fi Setup</h2>
    <h2 data-lang="ru">&#x041D;&#x0430;&#x0441;&#x0442;&#x0440;&#x043E;&#x0439;&#x043A;&#x0430; Wi-Fi</h2>
    <p data-lang="he" class="show" style="direction:rtl;text-align:right">&#x05D4;&#x05D6;&#x05D9;&#x05E0;&#x05D5; &#x05D0;&#x05EA; &#x05E4;&#x05E8;&#x05D8;&#x05D9; &#x05D4;&#x05E8;&#x05E9;&#x05EA; &#x05D4;&#x05D1;&#x05D9;&#x05EA;&#x05D9;&#x05EA; &#x05E9;&#x05DC;&#x05DB;&#x05DD; &#x05DB;&#x05D3;&#x05D9; &#x05DC;&#x05D7;&#x05D1;&#x05E8; &#x05D0;&#x05EA; &#x05D4;&#x05E9;&#x05E2;&#x05D5;&#x05DF;.</p>
    <p data-lang="en">Enter your home network details to connect the watch.</p>
    <p data-lang="ru">&#x0412;&#x0432;&#x0435;&#x0434;&#x0438;&#x0442;&#x0435; &#x0434;&#x0430;&#x043D;&#x043D;&#x044B;&#x0435; &#x0434;&#x043E;&#x043C;&#x0430;&#x0448;&#x043D;&#x0435;&#x0439; &#x0441;&#x0435;&#x0442;&#x0438; &#x0434;&#x043B;&#x044F; &#x043F;&#x043E;&#x0434;&#x043A;&#x043B;&#x044E;&#x0447;&#x0435;&#x043D;&#x0438;&#x044F; &#x0447;&#x0430;&#x0441;&#x043E;&#x0432;.</p>
    <form action="/submit" method="POST">
      <label data-lang="he" class="show" style="direction:rtl;text-align:right">&#x05E9;&#x05DD; &#x05E8;&#x05E9;&#x05EA; (SSID):</label>
      <label data-lang="en">Wi-Fi Name (SSID):</label>
      <label data-lang="ru">&#x0418;&#x043C;&#x044F; &#x0441;&#x0435;&#x0442;&#x0438; (SSID):</label>
      <input type="text" id="ssid" name="ssid" required>
      <label data-lang="he" class="show" style="direction:rtl;text-align:right">&#x05E1;&#x05D9;&#x05E1;&#x05DE;&#x05D0; (&#x05D4;&#x05E9;&#x05D0;&#x05D9;&#x05E8;&#x05D5; &#x05E8;&#x05D9;&#x05E7; &#x05DC;&#x05E8;&#x05E9;&#x05EA; &#x05E4;&#x05EA;&#x05D5;&#x05D7;&#x05D4;):</label>
      <label data-lang="en">Password (leave empty for open networks):</label>
      <label data-lang="ru">&#x041F;&#x0430;&#x0440;&#x043E;&#x043B;&#x044C; (&#x043E;&#x0441;&#x0442;&#x0430;&#x0432;&#x044C;&#x0442;&#x0435; &#x043F;&#x0443;&#x0441;&#x0442;&#x044B;&#x043C; &#x0434;&#x043B;&#x044F; &#x043E;&#x0442;&#x043A;&#x0440;&#x044B;&#x0442;&#x044B;&#x0445; &#x0441;&#x0435;&#x0442;&#x0435;&#x0439;):</label>
      <input type="password" id="password" name="password" placeholder="">
      <button type="submit" class="btn btn-primary"><span data-lang="he" class="show">&#x05D4;&#x05EA;&#x05D7;&#x05D1;&#x05E8;</span><span data-lang="en">Connect</span><span data-lang="ru">&#x041F;&#x043E;&#x0434;&#x043A;&#x043B;&#x044E;&#x0447;&#x0438;&#x0442;&#x044C;</span></button>
    </form>
  </div>

  <div class="card" style="margin-top:4px">
    <div class="lang-bar" style="margin:0 0 12px 0">
      <button class="lang-btn active" onclick="setLang('he')">HE</button>
      <button class="lang-btn" onclick="setLang('en')">EN</button>
      <button class="lang-btn" onclick="setLang('ru')">RU</button>
    </div>
    <h2 data-lang="he" class="show" style="direction:rtl;text-align:right">&#x05E2;&#x05D6;&#x05E8;&#x05D4; &#x05DE;&#x05D4;&#x05D9;&#x05E8;&#x05D4;</h2>
    <h2 data-lang="en">Quick Help</h2>
    <h2 data-lang="ru">&#x041F;&#x043E;&#x043C;&#x043E;&#x0449;&#x044C;</h2>
    <div data-lang="he" class="show" style="direction:rtl;text-align:right">
      <p><strong>&#x05D0;&#x05D9;&#x05DA; &#x05DC;&#x05D4;&#x05EA;&#x05D7;&#x05D1;&#x05E8;:</strong> &#x05D4;&#x05D6;&#x05D9;&#x05E0;&#x05D5; &#x05E9;&#x05DD; &#x05E8;&#x05E9;&#x05EA; &#x05D5;&#x05E1;&#x05D9;&#x05E1;&#x05DE;&#x05D0; &#x05D5;&#x05DC;&#x05D7;&#x05E6;&#x05D5; &#x05E2;&#x05DC; &#x05D4;&#x05EA;&#x05D7;&#x05D1;&#x05E8;. &#x05D4;&#x05E9;&#x05E2;&#x05D5;&#x05DF; &#x05D9;&#x05D0;&#x05EA;&#x05D7;&#x05DC; &#x05DE;&#x05D7;&#x05D3;&#x05E9; &#x05D5;&#x05D9;&#x05EA;&#x05D7;&#x05D1;&#x05E8; &#x05DC;&#x05E8;&#x05E9;&#x05EA;.</p>
      <p><strong>&#x05E0;&#x05E7;&#x05D5;&#x05D3;&#x05D5;&#x05EA; &#x05D4;&#x05E0;&#x05E7;&#x05D5;&#x05D3;&#x05D4; &#x05DE;&#x05D4;&#x05D1;&#x05D4;&#x05D1;&#x05D5;&#x05EA; &#x05DE;&#x05D4;&#x05E8;?</strong> &#x05D0;&#x05D5;&#x05EA; Wi-Fi &#x05D0;&#x05D1;&#x05D3;. &#x05D4;&#x05E9;&#x05E2;&#x05D5;&#x05DF; &#x05DE;&#x05E0;&#x05E1;&#x05D4; &#x05DC;&#x05D4;&#x05EA;&#x05D7;&#x05D1;&#x05E8; &#x05DE;&#x05D7;&#x05D3;&#x05E9;. &#x05D1;&#x05D3;&#x05E7;&#x05D5; &#x05D0;&#x05EA; &#x05D4;&#x05E0;&#x05EA;&#x05D1;.</p>
      <p><strong>&#x05D1;&#x05D3;&#x05D9;&#x05E7;&#x05EA; &#x05E0;&#x05D5;&#x05E8;&#x05D5;&#x05EA;:</strong> &#x05D1;&#x05DB;&#x05DC; &#x05D0;&#x05EA;&#x05D7;&#x05D5;&#x05DC; &#x05DE;&#x05D7;&#x05D3;&#x05E9;, &#x05DB;&#x05DC; &#x05D4;&#x05E0;&#x05D5;&#x05E8;&#x05D5;&#x05EA; &#x05E0;&#x05D3;&#x05DC;&#x05E7;&#x05D5;&#x05EA; &#x05D0;&#x05D7;&#x05EA; &#x05D0;&#x05D7;&#x05EA;. &#x05D6;&#x05D4; &#x05E2;&#x05D5;&#x05D6;&#x05E8; &#x05DC;&#x05D5;&#x05D5;&#x05D3;&#x05D0; &#x05E9;&#x05DB;&#x05DC; &#x05D4;&#x05E0;&#x05D5;&#x05E8;&#x05D5;&#x05EA; &#x05EA;&#x05E7;&#x05D9;&#x05E0;&#x05D5;&#x05EA;.</p>
    </div>
    <div data-lang="en" class="en">
      <p><strong>How to connect:</strong> Enter your home WiFi name and password, then tap Connect. The watch will restart and connect to your network.</p>
      <p><strong>Colon LEDs blinking fast?</strong> WiFi signal was lost. The watch is trying to reconnect. Check your router.</p>
      <p><strong>LED test on reboot:</strong> Every time the watch restarts, all LEDs light up one by one. This helps verify all LEDs are working.</p>
    </div>
    <div data-lang="ru" class="ru">
      <p><strong>&#x041A;&#x0430;&#x043A; &#x043F;&#x043E;&#x0434;&#x043A;&#x043B;&#x044E;&#x0447;&#x0438;&#x0442;&#x044C;:</strong> &#x0412;&#x0432;&#x0435;&#x0434;&#x0438;&#x0442;&#x0435; &#x0438;&#x043C;&#x044F; &#x0438; &#x043F;&#x0430;&#x0440;&#x043E;&#x043B;&#x044C; WiFi, &#x043D;&#x0430;&#x0436;&#x043C;&#x0438;&#x0442;&#x0435; &#x041F;&#x043E;&#x0434;&#x043A;&#x043B;&#x044E;&#x0447;&#x0438;&#x0442;&#x044C;. &#x0427;&#x0430;&#x0441;&#x044B; &#x043F;&#x0435;&#x0440;&#x0435;&#x0437;&#x0430;&#x0433;&#x0440;&#x0443;&#x0437;&#x044F;&#x0442;&#x0441;&#x044F; &#x0438; &#x043F;&#x043E;&#x0434;&#x043A;&#x043B;&#x044E;&#x0447;&#x0430;&#x0442;&#x0441;&#x044F;.</p>
      <p><strong>&#x0422;&#x043E;&#x0447;&#x043A;&#x0438; &#x043C;&#x0438;&#x0433;&#x0430;&#x044E;&#x0442; &#x0431;&#x044B;&#x0441;&#x0442;&#x0440;&#x043E;?</strong> &#x041F;&#x043E;&#x0442;&#x0435;&#x0440;&#x044F;&#x043D; &#x0441;&#x0438;&#x0433;&#x043D;&#x0430;&#x043B; WiFi. &#x0427;&#x0430;&#x0441;&#x044B; &#x043F;&#x044B;&#x0442;&#x0430;&#x044E;&#x0442;&#x0441;&#x044F; &#x043F;&#x0435;&#x0440;&#x0435;&#x043F;&#x043E;&#x0434;&#x043A;&#x043B;&#x044E;&#x0447;&#x0438;&#x0442;&#x044C;&#x0441;&#x044F;. &#x041F;&#x0440;&#x043E;&#x0432;&#x0435;&#x0440;&#x044C;&#x0442;&#x0435; &#x0440;&#x043E;&#x0443;&#x0442;&#x0435;&#x0440;.</p>
      <p><strong>&#x0422;&#x0435;&#x0441;&#x0442; &#x0441;&#x0432;&#x0435;&#x0442;&#x043E;&#x0434;&#x0438;&#x043E;&#x0434;&#x043E;&#x0432;:</strong> &#x041F;&#x0440;&#x0438; &#x043A;&#x0430;&#x0436;&#x0434;&#x043E;&#x043C; &#x043F;&#x0435;&#x0440;&#x0435;&#x0437;&#x0430;&#x043F;&#x0443;&#x0441;&#x043A;&#x0435; &#x0432;&#x0441;&#x0435; &#x0441;&#x0432;&#x0435;&#x0442;&#x043E;&#x0434;&#x0438;&#x043E;&#x0434;&#x044B; &#x0437;&#x0430;&#x0433;&#x043E;&#x0440;&#x0430;&#x044E;&#x0442;&#x0441;&#x044F; &#x043F;&#x043E; &#x043E;&#x0447;&#x0435;&#x0440;&#x0435;&#x0434;&#x0438; &#x0434;&#x043B;&#x044F; &#x043F;&#x0440;&#x043E;&#x0432;&#x0435;&#x0440;&#x043A;&#x0438;.</p>
    </div>
  </div>

  <div class="foot">
    The Amazing Watch v3.0 &middot; <a href="https://www.instagram.com/ai.garage_" target="_blank">&#x05E2;&#x05E7;&#x05D1;&#x05D5; &#x05D0;&#x05D7;&#x05E8;&#x05D9;&#x05E0;&#x05D5; &#x05D1;&#x05D0;&#x05D9;&#x05E0;&#x05E1;&#x05D8;&#x05D2;&#x05E8;&#x05DD;</a>
  </div>
</div>

<script>
function setLang(l){
  document.querySelectorAll('[data-lang]').forEach(e=>e.classList.remove('show'));
  document.querySelectorAll('[data-lang="'+l+'"]').forEach(e=>e.classList.add('show'));
  document.querySelectorAll('.lang-btn').forEach(b=>b.classList.remove('active'));
  document.querySelector('.lang-btn[onclick*="'+l+'"]').classList.add('active');
  document.documentElement.lang=l==='he'?'he':l==='ru'?'ru':'en';
}
</script>
</body></html>
)=====";

static const char AP_DONE_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>The Amazing Watch</title>
<style>
:root{--bg:#0f0f23;--accent:#44d9e1;--accent2:#6e7dff}
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:#e0e0e0;min-height:100vh;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;padding:40px 20px}
h1{font-size:32px;background:linear-gradient(135deg,var(--accent2),var(--accent));-webkit-background-clip:text;-webkit-text-fill-color:transparent;font-weight:800}
p{font-size:18px;color:#999;margin-top:12px;line-height:1.6}
.check{font-size:64px;margin-bottom:20px}
.foot{margin-top:40px;font-size:12px;color:#555}
.foot a{color:var(--accent);text-decoration:none}
</style></head>
<body>
<div class="check">&#10003;</div>
<h1>Wi-Fi Configured!</h1>
<p>The watch will now connect to your network.</p><p style="margin-top:20px;font-size:16px;color:#e0e0e0">To access the control panel, open:<br><strong style="font-size:20px;color:var(--accent)">http://amazingwatch.local</strong><br><span style="font-size:13px;color:#666">The watch will also show the IP on its display</span></p>
<div class="foot">The Amazing Watch v3.0</div>
</body></html>
)=====";

// ======================== NVS ========================

bool WifiManager::loadCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);  // read-only
    m_ssid = prefs.getString("ssid", "");
    m_password = prefs.getString("pass", "");
    prefs.end();
    return m_ssid.length() > 0;
}

void WifiManager::saveCredentials(const String& ssid, const String& password) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putString("ssid", ssid);
    prefs.putString("pass", password);
    prefs.end();
    m_ssid = ssid;
    m_password = password;
}

void WifiManager::clearCredentials() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.remove("ssid");
    prefs.remove("pass");
    prefs.end();
    m_ssid = "";
    m_password = "";
}

// ======================== Core ========================

void WifiManager::begin() {
    if (loadCredentials()) {
        Serial.printf("Found stored WiFi: %s\n", m_ssid.c_str());
        startConnection();
    } else {
        Serial.println("No stored credentials, starting AP...");
        startCaptivePortal();
    }
}

void WifiManager::update() {
    unsigned long now = millis();

    switch (m_state) {
        case WIFI_STATE_CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                m_state = WIFI_STATE_CONNECTED;
                m_wifiWasConnected = true;
                m_justConnectedFlag = true;
                // Start mDNS so users can access via amazingwatch.local
                if (MDNS.begin("amazingwatch")) {
                    MDNS.addService("http", "tcp", 80);
                    Serial.println("mDNS: http://amazingwatch.local");
                }
                Serial.printf("WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
            } else if (now - m_connectStartTime > WIFI_CONNECT_TIMEOUT_MS) {
                Serial.println("WiFi connection timeout");
                WiFi.disconnect();
                // Open captive portal so user can enter new credentials if needed
                // but also keep retrying stored credentials in background (AP+STA mode)
                startCaptivePortal();
            }
            break;

        case WIFI_STATE_CONNECTED:
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("WiFi lost, will reconnect...");
                m_state = WIFI_STATE_RECONNECTING;
                m_lastReconnectAttempt = 0;  // trigger immediate attempt
            }
            break;

        case WIFI_STATE_RECONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                m_state = WIFI_STATE_CONNECTED;
                Serial.println("WiFi reconnected!");
            } else if (now - m_lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
                m_lastReconnectAttempt = now;
                Serial.println("Attempting WiFi reconnect...");
                WiFi.disconnect();
                WiFi.begin(m_ssid.c_str(), m_password.c_str());
            }
            break;

        case WIFI_STATE_AP_MODE:
            m_dnsServer.processNextRequest();
            m_dnsServer.processNextRequest();
            m_dnsServer.processNextRequest();
            // Check if stored WiFi connected in background (AP+STA mode)
            if (WiFi.status() == WL_CONNECTED && !m_apCredsReceived) {
                Serial.printf("Background WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
                stopCaptivePortal();
                m_state = WIFI_STATE_CONNECTED;
                m_wifiWasConnected = true;
                m_justConnectedFlag = true;
                if (MDNS.begin("amazingwatch")) { MDNS.addService("http", "tcp", 80); }
            }
            // Check if user submitted new credentials via portal
            else if (m_apCredsReceived) {
                m_state = WIFI_STATE_AP_GOT_CREDS;
            }
            // Auto-shutdown AP after 5 min if nobody connected to portal (save power)
            else if (m_ssid.length() > 0 && now - m_apStartTime > 300000) {
                Serial.println("AP timeout (5 min), switching to STA-only retry");
                stopCaptivePortal();
                m_state = WIFI_STATE_RECONNECTING;
                m_lastReconnectAttempt = 0;
                WiFi.mode(WIFI_STA);
                WiFi.begin(m_ssid.c_str(), m_password.c_str());
            }
            // Retry stored WiFi periodically while in AP mode
            else if (m_ssid.length() > 0 && now - m_lastReconnectAttempt > WIFI_RECONNECT_INTERVAL) {
                m_lastReconnectAttempt = now;
                WiFi.begin(m_ssid.c_str(), m_password.c_str());
            }
            break;

        case WIFI_STATE_AP_GOT_CREDS:
            // Small delay handled by state machine, not blocking delay()
            stopCaptivePortal();
            saveCredentials(m_apReceivedSSID, m_apReceivedPassword);
            startConnection();
            break;

        case WIFI_STATE_IDLE:
        default:
            break;
    }
}

void WifiManager::startConnection() {
    m_state = WIFI_STATE_CONNECTING;
    m_connectStartTime = millis();
    WiFi.mode(WIFI_STA);
    WiFi.begin(m_ssid.c_str(), m_password.c_str());
    Serial.printf("Connecting to: %s\n", m_ssid.c_str());
}

String WifiManager::getIP() const {
    if (m_state == WIFI_STATE_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "Not connected";
}

// ======================== Captive Portal ========================

void WifiManager::startCaptivePortal() {
    m_state = WIFI_STATE_AP_MODE;
    m_apCredsReceived = false;
    m_apStartTime = millis();

    // AP+STA mode: serve captive portal AND keep trying stored WiFi
    WiFi.mode(WIFI_AP_STA);
    const IPAddress apIP(4, 3, 2, 1);
    const IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, subnet);
    WiFi.softAP(AP_SSID, NULL, 6, 0, 4);
    // Also try connecting to stored WiFi in background
    if (m_ssid.length() > 0) {
        WiFi.begin(m_ssid.c_str(), m_password.c_str());
        Serial.printf("AP+STA: also trying %s in background\n", m_ssid.c_str());
    }

    m_dnsServer.setTTL(300);
    m_dnsServer.start(53, "*", apIP);

    if (m_apServer) {
        delete m_apServer;
    }
    m_apServer = new AsyncWebServer(WEB_PORT);

    String localURL = "http://4.3.2.1";

    // Captive portal detection — these trigger the OS popup
    // Windows 11
    m_apServer->on("/connecttest.txt", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    // Windows 10
    m_apServer->on("/wpad.dat", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(404); });
    m_apServer->on("/ncsi.txt", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    // Android
    m_apServer->on("/generate_204", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    m_apServer->on("/gen_204", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    // Apple iOS/macOS
    m_apServer->on("/hotspot-detect.html", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    m_apServer->on("/library/test/success.html", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    // Firefox
    m_apServer->on("/canonical.html", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    m_apServer->on("/success.txt", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    // General
    m_apServer->on("/redirect", HTTP_GET, [localURL](AsyncWebServerRequest* r) { r->redirect(localURL); });
    m_apServer->on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(404); });

    // Main page — serve the captive portal
    m_apServer->on("/", HTTP_ANY, [](AsyncWebServerRequest* r) {
        AsyncWebServerResponse* resp = r->beginResponse(200, "text/html", AP_HTML);
        resp->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        r->send(resp);
    });

    // Credential submission (POST, not GET — safer)
    m_apServer->on("/submit", HTTP_POST, [this](AsyncWebServerRequest* r) {
        if (r->hasParam("ssid", true)) {
            m_apReceivedSSID = r->getParam("ssid", true)->value();
            m_apReceivedPassword = r->hasParam("password", true) ? r->getParam("password", true)->value() : "";
            if (m_apReceivedSSID.length() == 0) {
                r->send(400, "text/plain", "SSID is required");
                return;
            }
            Serial.printf("AP received credentials for: %s (password: %s)\n",
                          m_apReceivedSSID.c_str(), m_apReceivedPassword.length() > 0 ? "yes" : "none");
            r->send(200, "text/html", AP_DONE_HTML);
            m_apCredsReceived = true;
        } else {
            r->send(400, "text/plain", "SSID is required");
        }
    });

    // Catch-all redirect
    m_apServer->onNotFound([localURL](AsyncWebServerRequest* r) {
        r->redirect(localURL);
    });

    m_apServer->begin();
    Serial.println("Captive portal started at 4.3.2.1");
}

void WifiManager::stopCaptivePortal() {
    m_dnsServer.stop();
    if (m_apServer) {
        m_apServer->end();
        delete m_apServer;
        m_apServer = nullptr;
    }
    WiFi.softAPdisconnect(true);
    Serial.println("Captive portal stopped");
}

bool WifiManager::justConnected() {
    if (m_justConnectedFlag) {
        m_justConnectedFlag = false;
        return true;
    }
    return false;
}

void WifiManager::setWifiPowerSave(bool enable) {
    esp_wifi_set_ps(WIFI_PS_NONE);
}
