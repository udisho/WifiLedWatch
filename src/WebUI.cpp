#include "WebUI.h"
#include "LedDisplay.h"
#include "TimeManager.h"
#include "ConfigStore.h"
#include "WifiManager.h"
#include "Heartbeat.h"
#include <Preferences.h>
#include <WiFi.h>
#include <Arduino.h>
#include <Update.h>

static int extractInt(const String& json, const char* key);
static String extractString(const String& json, const char* key);
static bool extractBool(const String& json, const char* key);

// Firmware magic marker — must exist in every valid NeoTick binary
#define OTA_MAGIC     "NEOTICK_FW_V3"
#define OTA_MAGIC_LEN 13
static const char NEOTICK_MAGIC[] __attribute__((used)) = OTA_MAGIC;
static const char OTA_PASSWORD[] = "neotick2024";

// OTA upload state
static bool otaMagicFound = false;
static bool otaPasswordOK = false;
static uint8_t otaTail[OTA_MAGIC_LEN];
static size_t otaTailLen = 0;

// === Mobile-first Web GUI ===
static const char WEB_HTML[] PROGMEM = R"=====(
<!DOCTYPE html><html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 50 50'%3E%3Ctext x='25' y='35' text-anchor='middle' font-family='monospace' font-size='20' font-weight='bold' fill='%2300ff88'%3E12:34%3C/text%3E%3C/svg%3E">
<title>NeoTick</title>
<style>
:root{--bg:#0f0f23;--card:#1a1a2e;--accent:#44d9e1;--accent2:#6e7dff;--text:#e0e0e0;--text2:#999;--btn:#2d2d44;--success:#4CAF50;--danger:#e74c3c;--work:#4CAF50;--rest:#e74c3c}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:var(--bg);color:var(--text);min-height:100vh;overflow-x:hidden;overscroll-behavior-y:contain}
.hdr{background:#0a0a1a;padding:22px 16px;text-align:center;position:relative;overflow:hidden;border-bottom:1px solid #1a1a2e}
.hdr::before{content:'';position:absolute;top:-50%;left:-50%;width:200%;height:200%;background:conic-gradient(from 0deg,transparent 0%,rgba(68,217,225,.06) 25%,transparent 50%,rgba(110,125,255,.06) 75%,transparent 100%);animation:headerShine 12s linear infinite}
@keyframes headerShine{0%{transform:rotate(0deg)}100%{transform:rotate(360deg)}}
.hdr h1{font-size:28px;color:#fff;font-weight:900;letter-spacing:3px;position:relative;text-transform:uppercase}
.hdr h1 span{color:var(--accent);font-weight:400}
.hdr .sub{font-size:11px;color:rgba(255,255,255,.4);margin-top:4px;position:relative;letter-spacing:1px}
.tabs{display:flex;background:var(--card);position:sticky;top:0;z-index:10;overflow-x:auto;-webkit-overflow-scrolling:touch}
.tab{flex:1 0 auto;padding:14px 10px;text-align:center;cursor:pointer;font-size:12px;font-weight:600;color:var(--text2);border-bottom:3px solid transparent;transition:.2s;white-space:nowrap}
.tab.active{color:var(--accent);border-bottom-color:var(--accent)}
.panel{display:none;padding:16px;padding-bottom:80px}
.panel.active{display:block}
.card{background:var(--card);border-radius:14px;padding:20px;margin-bottom:14px}
.card h3{font-size:13px;color:var(--accent);margin-bottom:14px;text-transform:uppercase;letter-spacing:1.5px;font-weight:700}
.big-time{font-size:56px;font-weight:800;text-align:center;font-family:'SF Mono','Courier New',monospace;color:#fff;letter-spacing:2px;padding:8px 0}
.big-time .sec{font-size:28px;color:var(--accent);vertical-align:baseline}
.big-time .blink{animation:blink 1s step-end infinite}
@keyframes blink{50%{opacity:.3}}
.sw-time{font-size:44px;font-weight:700;text-align:center;font-family:'SF Mono','Courier New',monospace;color:var(--accent);padding:16px 0}
.btn-row{display:flex;gap:10px;justify-content:center;flex-wrap:wrap;margin-top:14px}
.btn{padding:14px 28px;border:none;border-radius:12px;font-size:15px;cursor:pointer;font-weight:700;transition:.15s;touch-action:manipulation}
.btn:active{transform:scale(.96)}
.btn-primary{background:var(--accent);color:#000}
.btn-danger{background:var(--danger);color:#fff}
.btn-secondary{background:var(--btn);color:var(--text)}.btn-accent{background:#6e7dff;color:#fff}
.colors{display:grid;grid-template-columns:repeat(6,1fr);gap:8px;margin-top:10px;max-width:320px}
.color-dot{width:100%;max-width:44px;aspect-ratio:1;border-radius:50%;cursor:pointer;border:3px solid transparent;transition:.15s;touch-action:manipulation}
.color-dot:hover,.color-dot.active{border-color:#fff;transform:scale(1.15)}
.slider-row{display:flex;align-items:center;gap:12px;margin-top:12px}
.slider-row label{min-width:70px;font-size:13px;color:var(--text2)}
.slider-row input[type=range]{flex:1;height:6px;accent-color:var(--accent);-webkit-appearance:none;background:#333;border-radius:3px;outline:none}
.slider-row input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:var(--accent);cursor:pointer}
.slider-row .val{min-width:32px;text-align:right;font-size:14px;font-weight:600}
select{width:100%;padding:12px;border-radius:10px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:15px;margin-top:8px;-webkit-appearance:none;appearance:none}
.radio-group{display:flex;gap:14px;margin-top:10px;flex-wrap:wrap}
.radio-group label{display:flex;align-items:center;gap:6px;cursor:pointer;font-size:14px;touch-action:manipulation}
.status{font-size:11px;color:var(--text2);text-align:center;padding:6px}
.dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:4px}
.dot.on{background:var(--success)}.dot.off{background:var(--danger)}
.num-input{display:flex;gap:8px;justify-content:center;align-items:center;margin:14px 0;flex-wrap:wrap}
.num-input input{width:56px;padding:10px;text-align:center;border-radius:10px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:18px;font-weight:600}
.num-input span{font-size:13px;color:var(--text2)}
.custom-color{margin-top:12px;display:flex;align-items:center;gap:10px}
.custom-color input[type=color]{width:48px;height:40px;border:none;border-radius:10px;cursor:pointer;background:transparent}
.dst-rule{background:rgba(255,255,255,.03);border-radius:10px;padding:14px;margin-top:12px;border:1px solid #222}
.dst-rule h4{font-size:12px;color:var(--accent);margin-bottom:10px;font-weight:700}
.dst-rule .row{display:flex;gap:6px;align-items:center;margin-top:6px;flex-wrap:wrap}
.dst-rule select,.dst-rule input{padding:8px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:13px}
.tab-phase{font-size:18px;font-weight:700;text-align:center;padding:10px;border-radius:10px;margin-bottom:10px}
.tab-phase.work{background:rgba(76,175,80,.15);color:var(--work)}
.tab-phase.rest{background:rgba(231,76,60,.15);color:var(--rest)}
.tab-phase.done{background:rgba(68,217,225,.15);color:var(--accent)}
.tab-info{text-align:center;font-size:13px;color:var(--text2);margin-top:8px}
.toggle-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0}
.toggle-row span{font-size:14px}
.toggle{position:relative;width:48px;height:28px;cursor:pointer;touch-action:manipulation}
.toggle input{opacity:0;width:0;height:0}
.toggle .slider{position:absolute;inset:0;background:#444;border-radius:14px;transition:.2s}
.toggle .slider:before{content:'';position:absolute;width:22px;height:22px;left:3px;bottom:3px;background:#fff;border-radius:50%;transition:.2s}
.toggle input:checked+.slider{background:var(--accent)}
.toggle input:checked+.slider:before{transform:translateX(20px)}
.wifi-badge{display:inline-flex;align-items:center;gap:4px;font-size:11px;color:var(--text2);margin-top:4px}
.seg-wrap{display:flex;justify-content:center;align-items:center;gap:4px;padding:12px 0;overflow:hidden}
.seg-digit{position:relative;width:28px;height:50px}
.seg-digit span{position:absolute;background:var(--card);border-radius:2px;transition:background .15s}
.seg-digit span.on{background:var(--clr,var(--accent))}
.seg-digit .a{top:0;left:3px;width:22px;height:4px}
.seg-digit .b{top:3px;right:0;width:4px;height:21px}
.seg-digit .c{bottom:3px;right:0;width:4px;height:21px}
.seg-digit .d{bottom:0;left:3px;width:22px;height:4px}
.seg-digit .e{bottom:3px;left:0;width:4px;height:21px}
.seg-digit .f{top:3px;left:0;width:4px;height:21px}
.seg-digit .g{top:23px;left:3px;width:22px;height:4px}
.seg-colon{display:flex;flex-direction:column;gap:12px;justify-content:center;align-items:center;width:8px;height:50px}
.seg-colon i{width:6px;height:6px;border-radius:50%;background:var(--accent)}
@media(min-width:400px){.seg-digit{width:36px;height:64px}.seg-digit .a,.seg-digit .d,.seg-digit .g{left:4px;width:28px;height:5px}.seg-digit .b,.seg-digit .c{right:0;width:5px}.seg-digit .e,.seg-digit .f{left:0;width:5px}.seg-digit .b,.seg-digit .f{top:4px;height:26px}.seg-digit .c,.seg-digit .e{bottom:4px;height:26px}.seg-digit .g{top:29px}.seg-colon{width:10px;height:64px}.seg-colon i{width:7px;height:7px}}
.seg-bar{position:sticky;top:42px;z-index:9;background:var(--bg);padding:8px 0;border-bottom:1px solid #1a1a2e}
.wheel{display:flex;justify-content:center;align-items:center;gap:4px;margin:14px 0}
.wc{height:120px;width:56px;overflow-y:scroll;scroll-snap-type:y mandatory;-webkit-overflow-scrolling:touch;border-radius:10px;background:var(--btn);position:relative;padding:40px 0;scroll-padding:40px 0}
.wc div{height:40px;display:flex;align-items:center;justify-content:center;scroll-snap-align:center;font-size:20px;font-weight:600;color:var(--text)}
.wc-wrap{position:relative;display:inline-block}
.wc-wrap::after{content:'';position:absolute;top:50%;left:2px;right:2px;height:40px;transform:translateY(-50%);border:2px solid var(--accent);border-radius:8px;pointer-events:none;z-index:2}
.wc-label{font-size:12px;color:var(--text2);text-align:center;margin-top:2px}
.wheel-sep{font-size:24px;font-weight:700;color:var(--accent);padding:0 4px}
.seg-bar.anim .seg-digit span{animation:segAnim .3s infinite alternate}
@keyframes segAnim{0%{background:var(--accent)}50%{background:#6e7dff}100%{background:#ff6e7d}}
.seg-bar.paused .seg-wrap,.seg-bar.paused .big-time{animation:pausePulse 2s ease-in-out infinite}
@keyframes pausePulse{0%,100%{opacity:1}50%{opacity:.15}}
.sec-hdr{cursor:pointer;display:flex;justify-content:space-between;align-items:center;padding:14px 18px;background:var(--card);border-radius:14px;margin-bottom:2px}
.sec-hdr h3{margin:0;font-size:13px;color:var(--accent);text-transform:uppercase;letter-spacing:1.5px;font-weight:700}
.sec-hdr .arr{color:var(--text2);font-size:14px;transition:transform .2s}
.sec-hdr.open .arr{transform:rotate(180deg)}
.sec-body{display:none;background:var(--card);border-radius:0 0 14px 14px;padding:0 18px 18px;margin-top:-12px;margin-bottom:14px}
.sec-body.show{display:block}
.toast{position:fixed;top:50px;left:50%;transform:translateX(-50%);background:rgba(68,217,225,.9);color:#000;padding:6px 18px;border-radius:20px;font-size:12px;font-weight:700;z-index:99;opacity:0;transition:opacity .2s;pointer-events:none}
.toast.show{opacity:1}
</style>
</head>
<body>
<div class="hdr">
  <h1>NEO<span>TICK</span></h1>
  <div class="sub"><a href="https://www.instagram.com/ai.garage_" target="_blank" style="color:rgba(255,255,255,.4);text-decoration:none;letter-spacing:1px">by The AI Garage</a></div>
</div>
<div class="tabs">
  <div class="tab active" data-tab="clock">Clock</div>
  <div class="tab" data-tab="stopwatch">Stopwatch</div>
  <div class="tab" data-tab="timer">Timer</div>
  <div class="tab" data-tab="tabata">Tabata</div>
  <div class="tab" data-tab="pomodoro">Pomodoro</div>
  <div class="tab" data-tab="settings">Settings</div>
</div>
<div class="seg-bar"><div class="seg-wrap" id="segDisp"><div class="seg-digit" id="sd0"></div><div class="seg-digit" id="sd1"></div><div class="seg-colon"><i></i><i></i></div><div class="seg-digit" id="sd2"></div><div class="seg-digit" id="sd3"></div></div><div class="big-time" id="timeDisp">--<span class="blink">:</span>--<span class="sec">:--</span></div></div>

<div class="panel active" id="clock">
  <div class="card">
    <div class="status"><span class="dot" id="syncDot"></span><span id="syncText">Syncing...</span>
      <span class="wifi-badge"><span class="dot" id="wifiDot"></span><span id="wifiText">WiFi</span></span>
    </div>
  </div>
  <div class="card">
    <h3>Display Format</h3>
    <div class="toggle-row">
      <span id="fmtLabel">HH:MM</span>
      <label class="toggle"><input type="checkbox" id="mmssToggle" onchange="toggleMMSS()"><span class="slider"></span></label>
    </div>
  </div>
  <div class="card">
    <h3>Transition Animation</h3>
    <div class="toggle-row">
      <span id="animLabel">Fade on digit change</span>
      <label class="toggle"><input type="checkbox" id="animToggle" checked onchange="toggleAnim()"><span class="slider"></span></label>
    </div>
  </div>
  <div class="card" style="text-align:center">
    <button class="btn btn-primary" onclick="send({cmd:'animate'})">LED Test</button>
  </div>
</div>

<div class="panel" id="stopwatch">
  <div class="card">
    <div class="sw-time" id="swDisp">00:00.0</div>
    <div class="btn-row">
      <button class="btn" id="swToggle" onclick="swToggle()">Start</button>
      <button class="btn btn-accent" id="swBcastBtn" onclick="swToggle(true)" style="display:none;font-size:11px;padding:8px 12px">All Watches</button>
      <button class="btn btn-secondary" id="swReset2" onclick="send({cmd:'sw',action:'reset'})" style="display:none">Reset</button>
    </div>
  </div>
</div>

<div class="panel" id="timer">
  <div class="card">
    <div class="sw-time" id="timerDisp">01:00</div>
    <div class="wheel" id="timerSetRow"><div><div class="wc-wrap"><div class="wc" id="timerMinW"></div></div><div class="wc-label">min</div></div><div class="wheel-sep">:</div><div><div class="wc-wrap"><div class="wc" id="timerSecW"></div></div><div class="wc-label">sec</div></div></div>
    <div class="btn-row">
      <button class="btn btn-secondary" id="tmSetBtn" onclick="tmSet()">Set</button>
      <button class="btn" id="tmToggle" onclick="tmToggle()">Start</button>
      <button class="btn btn-accent" id="tmBcastBtn" onclick="tmToggle(true)" style="display:none;font-size:11px;padding:8px 12px">All Watches</button>
    </div>
  </div>
</div>

<div class="panel" id="tabata">
  <div class="card" style="padding:12px 16px">
    <div style="display:flex;align-items:center;justify-content:space-between;gap:8px;flex-wrap:wrap">
      <div style="flex:1;min-width:120px">
        <div class="tab-phase" id="tabPhase" style="font-size:15px;padding:6px;margin-bottom:4px">READY</div>
        <div class="sw-time" id="tabDisp" style="font-size:36px;padding:4px 0">00:20</div>
        <div class="tab-info" id="tabInfo" style="font-size:12px;margin-top:2px">Interval: - / -</div>
      </div>
      <div style="display:flex;flex-direction:column;gap:6px">
        <button class="btn" id="tabToggle" onclick="tabToggle()" style="padding:10px 24px;font-size:14px">Start</button>
        <button class="btn btn-accent" id="tabBcastBtn" onclick="tabToggle(true)" style="display:none;font-size:11px;padding:6px 12px">All Watches</button>
        <button class="btn btn-secondary" id="tabReset2" onclick="send({cmd:'tabata',action:'reset'})" style="display:none;padding:8px 20px;font-size:12px">Reset</button>
      </div>
    </div>
    <div id="tabSummary" style="display:none;text-align:center;font-size:13px;color:var(--text2);margin-top:8px;padding-top:8px;border-top:1px solid #222">
      Work: <strong id="tabSumWork" style="color:var(--work)">20s</strong> &middot;
      Rest: <strong id="tabSumRest" style="color:var(--rest)">10s</strong> &middot;
      Intervals: <strong id="tabSumInt" style="color:var(--accent)">8</strong>
    </div>
  </div>
  <div class="card" id="tabCfg">
    <h3>Tabata Settings</h3>
    <div style="display:flex;gap:24px;justify-content:center;align-items:flex-start;flex-wrap:wrap">
      <div style="text-align:center"><div style="font-size:11px;color:var(--work);margin-bottom:2px">Work</div><div style="display:flex;gap:2px;align-items:center"><div class="wc-wrap"><div class="wc" id="tabWorkMinW" style="width:44px;height:100px"></div></div><span style="font-size:11px;color:var(--text2)">:</span><div class="wc-wrap"><div class="wc" id="tabWorkSecW" style="width:44px;height:100px"></div></div></div><div style="font-size:10px;color:var(--text2)">min : sec</div></div>
      <div style="text-align:center"><div style="font-size:11px;color:var(--rest);margin-bottom:2px">Rest</div><div style="display:flex;gap:2px;align-items:center"><div class="wc-wrap"><div class="wc" id="tabRestMinW" style="width:44px;height:100px"></div></div><span style="font-size:11px;color:var(--text2)">:</span><div class="wc-wrap"><div class="wc" id="tabRestSecW" style="width:44px;height:100px"></div></div></div><div style="font-size:10px;color:var(--text2)">min : sec</div></div>
      <div style="text-align:center"><div style="font-size:11px;color:var(--accent);margin-bottom:2px">Rounds</div><div class="wc-wrap"><div class="wc" id="tabIntW" style="width:44px;height:100px"></div></div><div style="font-size:10px;color:var(--text2)">&nbsp;</div></div>
    </div>
    <div style="display:flex;gap:8px;margin-top:12px;justify-content:center;flex-wrap:wrap">
      <span style="font-size:12px;color:var(--text2)">Work:</span><select id="tabWC" style="width:auto;padding:4px 8px;font-size:12px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text)"></select>
      <span style="font-size:12px;color:var(--text2)">Rest:</span><select id="tabRC" style="width:auto;padding:4px 8px;font-size:12px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text)"></select>
    </div>
    <div class="btn-row" style="margin-top:12px">
      <button class="btn btn-primary" style="font-size:13px;padding:10px 20px" onclick="saveTabata()">Save</button>
    </div>
    <div style="margin-top:12px;border-top:1px solid #222;padding-top:10px">
      <h3 style="font-size:12px;margin-bottom:8px">Presets</h3>
      <div id="tabPresetList" style="margin-bottom:8px"></div>
      <select id="tabPresetSel" style="display:none"></select>
      <div style="display:flex;gap:6px;align-items:center">
        <input type="text" id="tabPresetName" maxlength="15" placeholder="Preset name" style="flex:1;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
        <button class="btn btn-primary" style="padding:6px 10px;font-size:11px" onclick="saveTabPreset()">Save Preset</button>
      </div>
    </div>
  </div>
</div>


<div class="panel" id="pomodoro">
  <div class="card">
    <div class="tab-phase" id="pomPhase">READY</div>
    <div class="sw-time" id="pomDisp">25:00</div>
    <div class="tab-info" id="pomInfo">Interval: 1 / 4</div>
    <div class="btn-row">
      <button class="btn" id="pomToggle" onclick="pomToggle()">Start</button>
      <button class="btn btn-accent" id="pomBcastBtn" onclick="pomToggle(true)" style="display:none;font-size:11px;padding:8px 12px">All Watches</button>
      <button class="btn btn-secondary" id="pomReset2" onclick="send({cmd:'pom',action:'reset'})" style="display:none">Reset</button>
    </div>
  </div>
  <div class="card">
    <h3>Pomodoro Settings</h3>
    <div class="slider-row"><label>Intervals</label><input type="range" id="pomIntSlider" min="1" max="8" value="4"><span class="val" id="pomIntVal">4</span></div>
    <div class="btn-row" style="margin-top:10px"><button class="btn btn-primary" onclick="savePomInt()">Save</button></div>
  </div>
</div>

<div class="panel" id="settings">
  <div class="sec-hdr" onclick="togSec(this)"><h3>Color &amp; Brightness</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="colors" id="colorGrid"></div>
    <div class="custom-color">
      <span style="font-size:13px;color:var(--text2)">Custom:</span>
      <input type="color" id="customColor" value="#00ff00">
      <button class="btn btn-secondary" style="padding:10px 16px;font-size:13px" onclick="applyCustomColor()">Apply</button>
    </div>
    <div class="slider-row" style="margin-top:14px">
      <label>Brightness</label>
      <input type="range" id="brightSlider" min="5" max="230" value="100">
      <span class="val" id="brightVal">100</span>
    </div>
    <div style="margin-top:14px">
      <h3 style="font-size:12px;margin-bottom:8px">Color Mode</h3>
      <div class="radio-group">
        <label><input type="radio" name="clrMode" value="0" checked onchange="send({cmd:'colormode',value:0})">Static</label>
        <label><input type="radio" name="clrMode" value="1" onchange="send({cmd:'colormode',value:1})">Rainbow</label>
        <label><input type="radio" name="clrMode" value="2" onchange="send({cmd:'colormode',value:2})">Crazy</label>
        <label><input type="radio" name="clrMode" value="3" onchange="send({cmd:'colormode',value:3})">Pulse</label>
      </div>
    </div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Timezone</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <select id="tzSelect" onchange="setTimezone()"></select>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Night Shift & Colon LEDs</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="toggle-row"><span>Colon LEDs (seconds dots)</span><label class="toggle"><input type="checkbox" id="colonToggle" checked onchange="send({cmd:'colon',enabled:this.checked})"><span class="slider"></span></label></div>
    <div style="font-size:11px;color:var(--text2);margin:4px 0 12px">Auto-off during night shift</div>
    <div class="toggle-row"><span>Auto-dim at night</span><label class="toggle"><input type="checkbox" id="nsToggle" onchange="saveNightShift()"><span class="slider"></span></label></div>
    <div class="slider-row" style="margin-top:10px"><label>Start</label><select id="nsStart" onchange="saveNightShift()" style="width:80px"></select><label>End</label><select id="nsEnd" onchange="saveNightShift()" style="width:80px"></select></div>
    <div class="slider-row"><label>Brightness</label><input type="range" id="nsBright" min="5" max="80" value="15" onchange="saveNightShift()"><span class="val" id="nsBrightVal">15</span></div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Buzzer</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="radio-group">
      <label><input type="radio" name="buzz" value="0" onchange="send({cmd:'buzzer',level:0})">Off</label>
      <label><input type="radio" name="buzz" value="1" onchange="send({cmd:'buzzer',level:1})">Low</label>
      <label><input type="radio" name="buzz" value="2" checked onchange="send({cmd:'buzzer',level:2})">High</label>
    </div>
    <div class="toggle-row" style="margin-top:10px"><span>Clockwork chime (hourly)</span><label class="toggle"><input type="checkbox" id="cwToggle" onchange="send({cmd:'clockwork',enabled:this.checked})"><span class="slider"></span></label></div>
    <div style="font-size:11px;color:var(--text2);margin:2px 0 10px">Chimes the hour count. Silent during night shift.</div>
    <div style="text-align:center"><button class="btn btn-secondary" style="padding:8px 16px;font-size:12px" onclick="send({cmd:'buzztest'})">Test Buzzer</button></div>
  </div>


  <div class="sec-hdr" onclick="togSec(this)"><h3>Info Display</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="toggle-row"><span>Show date</span><label class="toggle"><input type="checkbox" id="dateToggle" onchange="saveInfo()"><span class="slider"></span></label></div>
    <div class="toggle-row"><span>Show temperature</span><label class="toggle"><input type="checkbox" id="tempToggle" onchange="saveInfo()"><span class="slider"></span></label></div>
    <div class="slider-row"><label>Interval</label><input type="range" id="dateIntSlider" min="10" max="120" value="30" onchange="saveInfo()"><span class="val" id="dateIntVal">30</span><span style="font-size:11px;color:var(--text2)">sec</span></div>
    <div style="margin-top:8px">
      <div class="radio-group">
        <label><input type="radio" name="tempType" value="0" checked onchange="saveInfo()">Actual</label>
        <label><input type="radio" name="tempType" value="1" onchange="saveInfo()">Feels like</label>
      </div>
      <div class="toggle-row"><span>Color by temperature</span><label class="toggle"><input type="checkbox" id="tempClrToggle" onchange="saveInfo()"><span class="slider"></span></label></div>
      <div id="tempReadout" style="font-size:12px;color:var(--accent);margin-top:6px"></div>
      <div style="margin-top:6px">
        <select id="locSelect" style="width:100%;padding:8px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:13px" onchange="setLoc()">
          <option value="auto">Auto (IP)</option>
          <option value="other">Other (lat/lon)...</option>
        </select>
        <div id="locManual" style="display:none;margin-top:6px;gap:6px;align-items:center">
          <input type="number" id="locLat" placeholder="Lat" step="0.01" style="width:45%;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
          <input type="number" id="locLon" placeholder="Lon" step="0.01" style="width:45%;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
          <button class="btn btn-primary" style="padding:6px 10px;font-size:11px" onclick="saveManualLoc()">Set</button>
        </div>
      </div>
      <div id="locStatus" style="font-size:11px;color:var(--text2);margin-top:4px"></div>
    </div>
  </div>


  <div class="sec-hdr" onclick="togSec(this)"><h3>Birthdays</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div style="font-size:11px;color:var(--text2);margin-bottom:10px;line-height:1.5">On a birthday, the watch scrolls "HAPPY BDAY [name]" every hour with a celebration animation.</div>
    <div id="bdayList" style="margin-bottom:8px"></div>
    <div style="display:flex;gap:6px;flex-wrap:wrap;align-items:center">
      <input type="text" id="bdayName" maxlength="15" placeholder="Name" style="width:80px;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
      <input type="number" id="bdayDay" min="1" max="31" placeholder="DD" style="width:44px;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
      <input type="number" id="bdayMon" min="1" max="12" placeholder="MM" style="width:44px;padding:6px;border-radius:6px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:12px">
      <button class="btn btn-primary" style="padding:6px 12px;font-size:12px" onclick="addBday()">Add</button>
    </div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>DST Rules</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body">
    <div class="radio-group">
      <label><input type="radio" name="dst" value="0" onchange="setDST(0)">Off</label>
      <label><input type="radio" name="dst" value="1" checked onchange="setDST(1)">Custom</label>
      <label><input type="radio" name="dst" value="2" onchange="setDST(2)">Always On</label>
    </div>
    <div id="dstRules">
      <div class="dst-rule"><h4>Start (winter &rarr; summer)</h4><div class="row"><select id="dsFL"><option value="1">Last</option><option value="0">First</option></select><select id="dsDow"></select><span style="color:var(--text2)">of</span><select id="dsMon"></select><span style="color:var(--text2)">at</span><input type="number" id="dsHour" value="2" min="0" max="23" style="width:44px"><span style="color:var(--text2)">:00</span></div></div>
      <div class="dst-rule"><h4>End (summer &rarr; winter)</h4><div class="row"><select id="deFL"><option value="1">Last</option><option value="0">First</option></select><select id="deDow"></select><span style="color:var(--text2)">of</span><select id="deMon"></select><span style="color:var(--text2)">at</span><input type="number" id="deHour" value="2" min="0" max="23" style="width:44px"><span style="color:var(--text2)">:00</span></div></div>
      <div class="btn-row" style="margin-top:10px"><button class="btn btn-primary" style="font-size:12px" onclick="saveDSTRules()">Save</button><button class="btn btn-secondary" style="font-size:12px" onclick="resetDSTIsrael()">Israel Default</button></div>
    </div>
  </div>

  <div class="sec-hdr" onclick="togSec(this)"><h3>Firmware Update</h3><span class="arr">&#9660;</span></div>
  <div class="sec-body" id="otaSec">
    <div style="background:rgba(231,76,60,.1);border:1px solid rgba(231,76,60,.3);border-radius:10px;padding:12px;margin-bottom:12px">
      <div style="font-size:13px;color:var(--danger);font-weight:700;margin-bottom:4px">&#9888; Developer Only</div>
      <div style="font-size:11px;color:var(--text2);line-height:1.5">Uploading incorrect firmware can brick your device. Do not use unless you know what you are doing. Do not disconnect power during update.</div>
    </div>
    <div id="otaForm">
      <div style="margin-bottom:10px"><input type="password" id="otaPass" placeholder="Developer password" style="width:100%;padding:10px;border-radius:8px;border:1px solid #333;background:var(--btn);color:var(--text);font-size:14px"></div>
      <div style="margin-bottom:10px"><input type="file" id="otaFile" accept=".bin" style="font-size:13px;color:var(--text2)"></div>
      <button class="btn btn-danger" style="width:100%;padding:12px;font-size:14px" onclick="startOTA()">Upload Firmware</button>
    </div>
    <div id="otaProgress" style="display:none;text-align:center">
      <div style="font-size:16px;font-weight:700;color:var(--accent);margin-bottom:10px" id="otaStatus">Uploading...</div>
      <div style="background:#333;border-radius:6px;height:8px;overflow:hidden;margin-bottom:8px"><div id="otaBar" style="background:var(--accent);height:100%;width:0%;transition:width .2s"></div></div>
      <div style="font-size:12px;color:var(--text2)" id="otaPct">0%</div>
    </div>
  </div>

  <div class="card">
    <h3>WiFi</h3>
    <p style="font-size:14px;color:var(--text2)">SSID: <strong id="wifiSSID">--</strong></p>
    <p style="font-size:14px;color:var(--text2);margin-top:6px">IP: <strong id="wifiIP">--</strong></p>
    <p style="font-size:14px;color:var(--text2);margin-top:6px" id="rssiLine">Signal: --</p>
    <p style="font-size:14px;color:var(--text2);margin-top:6px" id="tempLine">CPU Temp: --</p>
    <div class="btn-row" style="margin-top:10px"><button class="btn btn-danger" style="font-size:12px;padding:8px 14px" onclick="resetWifi()">Reset WiFi</button></div>
  </div>

  <div class="card">
    <div class="status">v3.0 &middot; NeoTick &middot; <a href="https://www.instagram.com/ai.garage_" target="_blank" style="color:var(--accent);text-decoration:none">The AI Garage</a></div>
  </div>
</div>
<div class="toast" id="toast">Saving...</div>

<script>
const C=[
  {n:"Red",c:"#FF0000"},{n:"Green",c:"#008000"},{n:"Blue",c:"#0000FF"},
  {n:"Yellow",c:"#FFFF00"},{n:"Cyan",c:"#00FFFF"},{n:"Magenta",c:"#FF00FF"},
  {n:"Orange",c:"#FFA500"},{n:"Purple",c:"#800080"},{n:"Aqua",c:"#00FFFF"},
  {n:"Lime",c:"#00FF00"},{n:"Indigo",c:"#4B0082"},{n:"Teal",c:"#008080"},
  {n:"Turquoise",c:"#40E0D0"},{n:"Gold",c:"#FFD700"},{n:"Maroon",c:"#800000"},
  {n:"Olive",c:"#808000"},{n:"Navy",c:"#000080"},{n:"SkyBlue",c:"#87CEEB"},
  {n:"Coral",c:"#FF7F50"},{n:"Lavender",c:"#E6E6FA"},{n:"Silver",c:"#C0C0C0"},
  {n:"Pink",c:"#FFC0CB"},{n:"White",c:"#FFFFFF"}
];
const DAYS=["Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"];
const MON=["","Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"];
const TZ=[
  ["UTC-12 Baker Is.",-43200],["UTC-11 Samoa",-39600],["UTC-10 Hawaii",-36000],
  ["UTC-9 Alaska",-32400],["UTC-8 Pacific US",-28800],["UTC-7 Mountain US",-25200],
  ["UTC-6 Central US",-21600],["UTC-5 Eastern US",-18000],["UTC-4 Atlantic",-14400],
  ["UTC-3 Buenos Aires",-10800],["UTC-2 Mid-Atlantic",-7200],["UTC-1 Azores",-3600],
  ["UTC+0 London/GMT",0],["UTC+1 Paris/Berlin",3600],["UTC+2 Israel/Helsinki",7200],
  ["UTC+3 Moscow",10800],["UTC+3:30 Tehran",12600],["UTC+4 Dubai",14400],
  ["UTC+5 Karachi",18000],["UTC+5:30 Mumbai",19800],["UTC+6 Dhaka",21600],
  ["UTC+7 Bangkok",25200],["UTC+8 Singapore",28800],["UTC+9 Tokyo",32400],
  ["UTC+9:30 Adelaide",34200],["UTC+10 Sydney",36000],["UTC+11 Solomon",39600],
  ["UTC+12 Auckland",43200]
];
let ws,st={},firstState=true,wheelsInit=false,lastMode=-1,saving=false,prev={};
const SEG=[0x7E,0x30,0x6D,0x79,0x33,0x5B,0x5F,0x70,0x7F,0x7B];
const SEG_DEG=0x63;
const SEGS='abcdefg';
function initSegs(){for(let i=0;i<4;i++){const el=document.getElementById('sd'+i);el.innerHTML='';SEGS.split('').forEach(s=>{const sp=document.createElement('span');sp.className=s;el.appendChild(sp);});}}
function setDigit(idx,val){const el=document.getElementById('sd'+idx);if(!el)return;const bits=val>=0&&val<=9?SEG[val]:0;const spans=el.querySelectorAll('span');SEGS.split('').forEach((s,i)=>{spans[i].classList.toggle('on',!!(bits&(0x40>>i)));});}
function setDigitRaw(idx,bits){const el=document.getElementById('sd'+idx);if(!el)return;const spans=el.querySelectorAll('span');SEGS.split('').forEach((s,i)=>{spans[i].classList.toggle('on',!!(bits&(0x40>>i)));});}
const SEG_MINUS=0x01;
function updateSeg(){if(st.dv===undefined)return;const v=st.dv;if(st.db){setDigit(0,-1);setDigit(1,-1);setDigit(2,-1);setDigit(3,-1);}else if(st.dt){var neg=v<0,abs=Math.abs(v);if(abs>99)abs=99;if(neg){setDigitRaw(0,SEG_MINUS);if(abs>=10){setDigit(1,Math.floor(abs/10));setDigit(2,abs%10);setDigitRaw(3,SEG_DEG);}else{setDigit(1,abs);setDigitRaw(2,SEG_DEG);setDigit(3,-1);}}else{if(abs>=10){setDigit(0,Math.floor(abs/10));setDigit(1,abs%10);}else{setDigit(0,-1);setDigit(1,abs);}setDigitRaw(2,SEG_DEG);setDigit(3,-1);}}else{setDigit(0,Math.floor(v/1000)%10);setDigit(1,Math.floor(v/100)%10);setDigit(2,Math.floor(v/10)%10);setDigit(3,v%10);}}
function hslStr(h,s,l){return 'hsl('+h+','+s+'%,'+l+'%)';}
var crazyHues=[0,0,0,0],lastCrazyT=0,pulseHue=0,pulsePhaseStart=0,pulseTransitioning=false;
function animSegColors(){var now=Date.now();if(st.clrMode===2){if(now-lastCrazyT>200){lastCrazyT=now;for(var i=0;i<4;i++)crazyHues[i]=Math.floor(Math.random()*360);}for(var i=0;i<4;i++)document.getElementById('sd'+i).style.setProperty('--clr',hslStr(crazyHues[i],100,50));}else if(st.clrMode===3){if(!pulsePhaseStart)pulsePhaseStart=now;var h;if(!pulseTransitioning){h=pulseHue;if(now-pulsePhaseStart>=23000){pulseTransitioning=true;pulsePhaseStart=now;}}else{var el=now-pulsePhaseStart;if(el>=3000){pulseHue=(pulseHue+25)%360;h=pulseHue;pulseTransitioning=false;pulsePhaseStart=now;}else{var t=el/3000;var e=t*t*(3-2*t);h=(pulseHue+Math.floor(25*e))%360;}}for(var i=0;i<4;i++)document.getElementById('sd'+i).style.setProperty('--clr',hslStr(h,100,50));}}
function makeWheel(id,max){const el=document.getElementById(id);el.innerHTML='';for(let i=0;i<=max;i++){const d=document.createElement('div');d.textContent=String(i);el.appendChild(d);}}
function setWheel(id,val){const el=document.getElementById(id);setTimeout(()=>{el.scrollTop=val*40;},50);}
function getWheel(id){return Math.max(0,Math.round(document.getElementById(id).scrollTop/40));}
function init(){
  initSegs();
  makeWheel('timerMinW',59);makeWheel('timerSecW',59);
  makeWheel('tabWorkMinW',10);makeWheel('tabWorkSecW',59);
  makeWheel('tabRestMinW',10);makeWheel('tabRestSecW',59);
  makeWheel('tabIntW',20);
  setWheel('timerMinW',1);setWheel('timerSecW',0);
  document.querySelectorAll('.tab').forEach(t=>{
    t.onclick=()=>{
      const dest=t.dataset.tab;
      const m={clock:0,stopwatch:1,timer:2,tabata:3,pomodoro:4,settings:-1};
      const destMode=m[dest];
      if(destMode!==undefined&&destMode>=0){
        const running=(st.swRun&&dest!=='stopwatch')||(st.tmRun&&dest!=='timer')||(st.tabRun&&dest!=='tabata');
        if(running){
          const what=st.swRun?'Stopwatch':st.tmRun?'Timer':'Tabata';
          if(!confirm(what+' is running. Switching will stop it. Continue?'))return;
          if(st.swRun)send({cmd:'sw',action:'stop'});
          if(st.tmRun)send({cmd:'timer',action:'stop'});
          if(st.tabRun)send({cmd:'tabata',action:'stop'});
        }
      }
      document.querySelectorAll('.tab,.panel').forEach(e=>e.classList.remove('active'));
      t.classList.add('active');document.getElementById(dest).classList.add('active');
      if(destMode>=0)send({cmd:'mode',value:destMode});
    };
  });
  const g=document.getElementById('colorGrid');
  C.forEach((c,i)=>{const d=document.createElement('div');d.className='color-dot';d.style.background=c.c;d.title=c.n;d.onclick=()=>send({cmd:'color',index:i});g.appendChild(d);});
  const bs=document.getElementById('brightSlider');
  bs.oninput=()=>{document.getElementById('brightVal').textContent=bs.value;};
  bs.onchange=()=>send({cmd:'brightness',value:+bs.value});
  const tz=document.getElementById('tzSelect');
  TZ.forEach(([l,o])=>{const op=document.createElement('option');op.value=o;op.textContent=l;tz.appendChild(op);});
  ['dsDow','deDow'].forEach(id=>{const s=document.getElementById(id);DAYS.forEach((d,i)=>{const o=document.createElement('option');o.value=i;o.textContent=d;s.appendChild(o);});});
  ['dsMon','deMon'].forEach(id=>{const s=document.getElementById(id);for(let i=1;i<=12;i++){const o=document.createElement('option');o.value=i;o.textContent=MON[i];s.appendChild(o);}});
  ['tabWC','tabRC'].forEach(id=>{const s=document.getElementById(id);C.forEach((c,i)=>{const o=document.createElement('option');o.value=i;o.textContent=c.n;s.appendChild(o);});});
  document.getElementById('tabWC').value=1;document.getElementById('tabRC').value=0;
  ['nsStart','nsEnd'].forEach(id=>{const s=document.getElementById(id);for(let i=0;i<24;i++){const o=document.createElement('option');o.value=i;o.textContent=P(i)+':00';s.appendChild(o);}});
  document.getElementById('nsStart').value=22;document.getElementById('nsEnd').value=7;
  document.getElementById('nsBright').oninput=function(){document.getElementById('nsBrightVal').textContent=this.value;};
  document.getElementById('pomIntSlider').oninput=function(){document.getElementById('pomIntVal').textContent=this.value;};
  document.getElementById('dateIntSlider').oninput=function(){document.getElementById('dateIntVal').textContent=this.value;};
  initCities();
  connectWS();
}
function connectWS(){
  const h=location.hostname||'4.3.2.1';
  ws=new WebSocket('ws://'+h+'/ws');
  ws.onmessage=e=>{try{st=JSON.parse(e.data);if(st.full&&saving){saving=false;document.getElementById('toast').classList.remove('show');}updateUI();}catch(x){}};
  ws.onclose=()=>setTimeout(connectWS,2000);
  ws.onerror=()=>ws.close();
}
function send(o){if(ws&&ws.readyState===1)ws.send(JSON.stringify(o));}
function sendSave(o){saving=true;document.getElementById('toast').classList.add('show');send(o);}
function togSec(el){el.classList.toggle('open');el.nextElementSibling.classList.toggle('show');}
function P(n){return String(n).padStart(2,'0');}
function updateUI(){var _sy=window.pageYOffset;
  if(firstState&&st.mode!==undefined){
    firstState=false;
    lastMode=st.mode;
    const tabs=['clock','stopwatch','timer','tabata','pomodoro'];
    const dest=tabs[st.mode]||'clock';
    document.querySelectorAll('.tab,.panel').forEach(e=>e.classList.remove('active'));
    document.querySelector('.tab[data-tab="'+dest+'"]').classList.add('active');
    document.getElementById(dest).classList.add('active');
  }
  if(st.mode!==undefined&&st.mode!==lastMode){
    lastMode=st.mode;
    const tabs=['clock','stopwatch','timer','tabata','pomodoro'];
    const dest=tabs[st.mode]||'clock';
    document.querySelectorAll('.tab,.panel').forEach(e=>e.classList.remove('active'));
    document.querySelector('.tab[data-tab="'+dest+'"]').classList.add('active');
    document.getElementById(dest).classList.add('active');
  }
  if(st.dv!==undefined){
    const t=document.getElementById('timeDisp');
    const v=st.dv;
    const d0=Math.floor(v/1000)%10,d1=Math.floor(v/100)%10,d2=Math.floor(v/10)%10,d3=v%10;
    var th;
    if(st.db){th='<span style="opacity:.3">--:--</span>';}
    else{th=P(d0*10+d1)+'<span class="blink">:</span>'+P(d2*10+d3);
      if(st.mode===0)th+=('<span class="sec">:'+P(st.s)+'</span>');
      if(st.mode===1)th+=('<span class="sec">.'+Math.floor(((st.swMs||0)%1000)/100)+'</span>');
    }
    if(prev.th!==th){prev.th=th;t.innerHTML=th;}
    if(prev.dv!==v||prev.db!==st.db){prev.dv=v;prev.db=st.db;updateSeg();}
    if(st.clrMode>=2){prev.clrMode=st.clrMode;animSegColors();t.style.color='var(--accent)';}else{if(prev.clrMode>=2){for(var i=0;i<4;i++)document.getElementById('sd'+i).style.removeProperty('--clr');prev.clrMode=st.clrMode;}if(st.clr&&prev.clr!==st.clr){prev.clr=st.clr;document.getElementById('segDisp').style.setProperty('--clr',st.clr);t.style.color=st.clr;}}
  }
  if(st.anim!==prev.anim){prev.anim=st.anim;document.querySelector('.seg-bar').classList.toggle('anim',!!st.anim);}
  var paused=(!st.swRun&&st.swMs>0&&st.mode===1)||(!st.tmRun&&!st.tmDone&&st.tmMs>0&&st.tmMs<st.tmDur&&st.mode===2)||(st.tabPaused&&st.mode===3);
  if(paused!==prev.paused){prev.paused=paused;document.querySelector('.seg-bar').classList.toggle('paused',paused);}
  if(st.synced!==prev.synced){prev.synced=st.synced;const sd=document.getElementById('syncDot'),stx=document.getElementById('syncText');if(st.synced){sd.className='dot on';stx.textContent='NTP OK';}else{sd.className='dot off';stx.textContent='Syncing...';}}
  if(st.wifiLost!==prev.wifiLost){prev.wifiLost=st.wifiLost;const wd=document.getElementById('wifiDot'),wt=document.getElementById('wifiText');if(st.wifiLost){wd.className='dot off';wt.textContent='WiFi Lost';}else{wd.className='dot on';wt.textContent='WiFi OK';}}
  if(st.full)document.querySelectorAll('.color-dot').forEach((d,i)=>d.classList.toggle('active',i===st.colorIdx));
  if(st.bright!==undefined&&st.full){document.getElementById('brightSlider').value=st.bright;document.getElementById('brightVal').textContent=st.bright;}
  if(st.mmss!==undefined&&st.full){
    document.getElementById('mmssToggle').checked=st.mmss;
    document.getElementById('fmtLabel').textContent=st.mmss?'MM:SS':'HH:MM';
  }
  if(st.swMs!==undefined){
    const ms=st.swMs,s=Math.floor(ms/1000),m=Math.floor(s/60);
    var swTxt=P(m)+':'+P(s%60)+'.'+Math.floor((ms%1000)/100);if(prev.swTxt!==swTxt){prev.swTxt=swTxt;document.getElementById('swDisp').textContent=swTxt;}
    var swBt,swBc;if(st.swRun){swBt='Stop';swBc='btn btn-danger';}else if(st.swMs>0){swBt='Resume';swBc='btn btn-primary';}else{swBt='Start';swBc='btn btn-primary';}
    if(prev.swBt!==swBt){prev.swBt=swBt;const b=document.getElementById('swToggle');b.textContent=swBt;b.className=swBc;}
    var swRstVis=(!st.swRun&&st.swMs>0)?'':'none';if(prev.swRst!==swRstVis){prev.swRst=swRstVis;document.getElementById('swReset2').style.display=swRstVis;}
  }
  if(st.tmMs!==undefined){
    var tmTxt;if(!st.tmRun&&(!st.tmMs||st.tmMs<=0)&&!st.tmDone){const m=getWheel('timerMinW'),s=getWheel('timerSecW');tmTxt=P(m)+':'+P(s);}else{const ms=Math.max(0,st.tmMs),s=Math.ceil(ms/1000),m=Math.floor(s/60);tmTxt=P(m)+':'+P(s%60);}
    if(prev.tmTxt!==tmTxt){prev.tmTxt=tmTxt;document.getElementById('timerDisp').textContent=tmTxt;}
    var tmBt,tmBc;if(st.tmRun){tmBt='Stop';tmBc='btn btn-danger';}else if(st.tmMs>0&&!st.tmDone){tmBt='Resume';tmBc='btn btn-primary';}else{tmBt='Start';tmBc='btn btn-primary';}
    if(prev.tmBt!==tmBt){prev.tmBt=tmBt;const b=document.getElementById('tmToggle');b.textContent=tmBt;b.className=tmBc;}
    if(st.tmRun!==prev.tmRun2){prev.tmRun2=st.tmRun;document.getElementById('timerSetRow').style.display=st.tmRun?'none':'';document.getElementById('tmSetBtn').style.display=st.tmRun?'none':'';}
  }
  if(st.tabMs!==undefined){
    const ms=Math.max(0,st.tabMs),s=Math.ceil(ms/1000),m=Math.floor(s/60);
    var tabTxt=P(m)+':'+P(s%60);if(prev.tabTxt!==tabTxt){prev.tabTxt=tabTxt;document.getElementById('tabDisp').textContent=tabTxt;}
    var phCls,phTxt;if(st.tabDone){phCls='tab-phase done';phTxt='DONE!';}else if(st.tabRun){phCls=st.tabWork?'tab-phase work':'tab-phase rest';phTxt=st.tabWork?'WORK':'REST';}else{phCls='tab-phase';phTxt='READY';}
    if(prev.phCls!==phCls){prev.phCls=phCls;const ph=document.getElementById('tabPhase');ph.className=phCls;ph.textContent=phTxt;}
    var tabInf='Interval: '+st.tabInt+' / '+(st.tabTotal||'?');if(prev.tabInf!==tabInf){prev.tabInf=tabInf;document.getElementById('tabInfo').textContent=tabInf;}
    var bTxt,bCls;if(st.tabRun){bTxt='Stop';bCls='btn btn-danger';}else{bTxt='Start';bCls='btn btn-primary';}
    if(prev.tabBtn!==bTxt){prev.tabBtn=bTxt;const b=document.getElementById('tabToggle');b.textContent=bTxt;b.className=bCls;}
    if(st.tabRun!==prev.tabRun||st.tabDone!==prev.tabDone){prev.tabRun=st.tabRun;prev.tabDone=st.tabDone;document.getElementById('tabReset2').style.display=(!st.tabRun&&(st.tabMs>0||st.tabDone))?'':'none';document.getElementById('tabCfg').style.display=st.tabRun?'none':'';}
  }
  if(st.tz!==undefined&&st.full)document.getElementById('tzSelect').value=st.tz;
  if(st.dst!==undefined&&st.full){document.querySelector('input[name=dst][value="'+st.dst+'"]').checked=true;document.getElementById('dstRules').style.display=st.dst==1?'':'none';}
  if(st.dsFL!==undefined&&st.full){
    document.getElementById('dsFL').value=st.dsFL?'1':'0';document.getElementById('dsDow').value=st.dsDow;
    document.getElementById('dsMon').value=st.dsMon;document.getElementById('dsHour').value=st.dsH;
    document.getElementById('deFL').value=st.deFL?'1':'0';document.getElementById('deDow').value=st.deDow;
    document.getElementById('deMon').value=st.deMon;document.getElementById('deHour').value=st.deH;
  }
  if(st.tbWork!==undefined&&!wheelsInit){
    wheelsInit=true;
    setWheel('tabWorkMinW',Math.floor(st.tbWork/60));setWheel('tabWorkSecW',st.tbWork%60);
    setWheel('tabRestMinW',Math.floor(st.tbRest/60));setWheel('tabRestSecW',st.tbRest%60);
    setWheel('tabIntW',st.tbInt2);document.getElementById('tabWC').value=st.tbWC;document.getElementById('tabRC').value=st.tbRC;
  }
  if(st.tbWork!==undefined){
    function fmtDur(s){return s>=60?(Math.floor(s/60)+'m'+((s%60)?((s%60)+'s'):'')):(s+'s');}
    document.getElementById('tabSumWork').textContent=fmtDur(st.tbWork);
    document.getElementById('tabSumRest').textContent=fmtDur(st.tbRest);
    document.getElementById('tabSumInt').textContent=st.tbInt2;
    var tsv=(st.tabRun||st.tabDone)?'block':'none';if(prev.tsv!==tsv){prev.tsv=tsv;document.getElementById('tabSummary').style.display=tsv;}
  }
  if(st.animTr!==undefined&&st.full){document.getElementById('animToggle').checked=st.animTr;}
  if(st.clrMode!==undefined&&st.full){var r2=document.querySelector('input[name=clrMode][value="'+st.clrMode+'"]');if(r2)r2.checked=true;}
  if(st.pomMs!==undefined){
    var ms=Math.max(0,st.pomMs),s=Math.ceil(ms/1000),m=Math.floor(s/60);
    var pomTxt=P(m)+':'+P(s%60);if(prev.pomTxt!==pomTxt){prev.pomTxt=pomTxt;document.getElementById('pomDisp').textContent=pomTxt;}
    var ppCls,ppTxt;if(st.pomDone){ppCls='tab-phase done';ppTxt='DONE!';}else if(st.pomRun){ppCls=st.pomWork?'tab-phase work':'tab-phase rest';ppTxt=st.pomWork?'FOCUS':'BREAK';}else{ppCls='tab-phase';ppTxt='READY';}
    if(prev.ppCls!==ppCls){prev.ppCls=ppCls;var ph=document.getElementById('pomPhase');ph.className=ppCls;ph.textContent=ppTxt;}
    var pomInf='Interval: '+st.pomInt+' / '+(st.pomTotal||4);if(prev.pomInf!==pomInf){prev.pomInf=pomInf;document.getElementById('pomInfo').textContent=pomInf;}
    var pBt;if(st.pomRun){pBt='Stop';}else{pBt='Start';}
    if(prev.pBt!==pBt){prev.pBt=pBt;var b=document.getElementById('pomToggle');b.textContent=pBt;b.className=st.pomRun?'btn btn-danger':'btn btn-primary';}
    if(st.pomRun!==prev.pomRun||st.pomDone!==prev.pomDone){prev.pomRun=st.pomRun;prev.pomDone=st.pomDone;document.getElementById('pomReset2').style.display=(!st.pomRun&&(st.pomMs>0||st.pomDone))?'':'none';}
  }
  if(st.pomTotal!==undefined&&st.full){document.getElementById('pomIntSlider').value=st.pomTotal;document.getElementById('pomIntVal').textContent=st.pomTotal;}
  if(st.dateEn!==undefined&&st.full){document.getElementById('dateToggle').checked=st.dateEn;document.getElementById('dateIntSlider').value=st.dateInt||30;document.getElementById('dateIntVal').textContent=st.dateInt||30;document.getElementById('tempToggle').checked=st.tempEn;var tr=document.querySelector('input[name=tempType][value="'+(st.tempFL?'1':'0')+'"]');if(tr)tr.checked=true;document.getElementById('tempClrToggle').checked=st.tempClr;}
  if(st.curTemp!==undefined){var el=document.getElementById('tempReadout');var txt='';if(st.curTemp!==null){txt=st.curTemp.toFixed(1)+'°C actual | '+st.curFL.toFixed(1)+'°C feels like';if(st.wLoc)txt+='\nLocation: '+st.wLoc;el.innerHTML=txt.replace('\n','<br>');}else{el.textContent='';}}
  if(st.full){var sel=document.getElementById('locSelect');if(st.wLat){var key=st.wLat.toFixed(2)+','+st.wLon.toFixed(2);sel.value=key;if(!sel.value||sel.value==='auto'){sel.value='other';document.getElementById('locManual').style.display='flex';document.getElementById('locLat').value=st.wLat;document.getElementById('locLon').value=st.wLon;}document.getElementById('locStatus').textContent='Location set'+(st.wLoc?' ('+st.wLoc+')':'');}else{sel.value='auto';}}

  if(st.colonEn!==undefined&&st.full)document.getElementById('colonToggle').checked=st.colonEn;
  if(st.buzzLv!==undefined&&st.full){var r=document.querySelector('input[name=buzz][value="'+st.buzzLv+'"]');if(r)r.checked=true;}
  if(st.cwBuzz!==undefined&&st.full)document.getElementById('cwToggle').checked=st.cwBuzz;
  if(st.rssi!==undefined){var r=st.rssi,q=r>-50?'Excellent':r>-65?'Good':r>-75?'Weak':'Poor',cl=r>-50?'var(--success)':r>-65?'var(--accent)':r>-75?'#FFA500':'var(--danger)';document.getElementById('rssiLine').innerHTML='Signal: <strong style="color:'+cl+'">'+r+' dBm ('+q+')</strong>';}
  if(st.bdays){var bl=document.getElementById('bdayList');bl.innerHTML='';st.bdays.forEach(function(b,i){bl.innerHTML+='<div style="display:flex;justify-content:space-between;align-items:center;padding:4px 0;font-size:13px"><span>'+b.n+' - '+P(b.d)+'/'+P(b.m)+'</span><button class="btn btn-danger" style="padding:4px 10px;font-size:11px" onclick="delBday('+i+')">X</button></div>';});}
  if(st.tabPresets){var sel=document.getElementById('tabPresetSel');sel.innerHTML='';var pl=document.getElementById('tabPresetList');pl.innerHTML='';st.tabPresets.forEach(function(p,i){if(p.n){var o=document.createElement('option');o.value=i;o.textContent=p.n;sel.appendChild(o);pl.innerHTML+='<div style="display:flex;justify-content:space-between;align-items:center;padding:6px 8px;margin-bottom:4px;background:rgba(255,255,255,.03);border-radius:8px;font-size:13px"><span style="color:var(--text)">'+p.n+' <span style="color:var(--text2);font-size:11px">'+p.w+'s / '+p.r+'s / '+p.i+'r</span></span><span style="display:flex;gap:4px"><button class="btn btn-secondary" style="padding:4px 10px;font-size:11px" onclick="loadTabPresetIdx('+i+')">Load</button><button class="btn btn-danger" style="padding:4px 8px;font-size:11px" onclick="delTabPresetIdx('+i+')">X</button></span></div>';}});if(!pl.innerHTML)pl.innerHTML='<div style="font-size:12px;color:var(--text2);padding:4px">No presets saved</div>';}
  if(st.nsEn!==undefined&&st.full){
    document.getElementById('nsToggle').checked=st.nsEn;
    document.getElementById('nsStart').value=st.nsStart;
    document.getElementById('nsEnd').value=st.nsEnd;
    document.getElementById('nsBright').value=st.nsBright;
    document.getElementById('nsBrightVal').textContent=st.nsBright;
  }
  if(st.temp!==undefined&&st.temp!==prev.temp){prev.temp=st.temp;var tc=st.temp,tq=tc<50?'Normal':tc<60?'Warm':tc<65?'Hot':'THROTTLED',tcl=tc<50?'var(--success)':tc<60?'#FFA500':'var(--danger)';var tl='CPU: <strong style="color:'+tcl+'">'+tc+'&deg;C ('+tq+')</strong>';if(st.thermThrot)tl+=' <span style="color:var(--danger);font-size:11px">&#9888; Brightness reduced</span>';document.getElementById('tempLine').innerHTML=tl;}
  if(st.ssid)document.getElementById('wifiSSID').textContent=st.ssid;
  if(st.ip)document.getElementById('wifiIP').textContent=st.ip;
  var hasPeers=st.peers&&st.peers>0;
  if(hasPeers!==prev.hasPeers){prev.hasPeers=hasPeers;['swBcastBtn','tmBcastBtn','tabBcastBtn','pomBcastBtn'].forEach(function(id){document.getElementById(id).style.display=hasPeers?'':'none';});}
  if(st.bcasting&&!prev.bcasting){prev.bcasting=true;document.getElementById('timeDisp').insertAdjacentHTML('afterend','<div id="bcastBadge" style="text-align:center;font-size:11px;color:#6e7dff;font-weight:700;margin-top:4px">BROADCASTING TO ALL</div>');}
  if(!st.bcasting&&prev.bcasting){prev.bcasting=false;var bb=document.getElementById('bcastBadge');if(bb)bb.remove();}
  if(window.pageYOffset!==_sy)window.scrollTo(0,_sy);
}
function swToggle(bc){
  if(st.swRun) send({cmd:'sw',action:'stop'});
  else send({cmd:'sw',action:'start',broadcast:!!bc});
}
function tmSet(){var d=(getWheel('timerMinW')*60+getWheel('timerSecW'))*1000;send({cmd:'timer',action:'set',duration:d});}
function tmToggle(bc){
  if(st.tmRun) send({cmd:'timer',action:'stop'});
  else if(st.tmMs>0&&!st.tmDone) send({cmd:'timer',action:'start',broadcast:!!bc});
  else send({cmd:'timer',action:'start',duration:(getWheel('timerMinW')*60+getWheel('timerSecW'))*1000,broadcast:!!bc});
}
function tabToggle(bc){
  if(st.tabRun) send({cmd:'tabata',action:'stop'});
  else send({cmd:'tabata',action:'start',broadcast:!!bc});
}

function saveTabata(){var ws=getWheel('tabWorkMinW')*60+getWheel('tabWorkSecW'),rs=getWheel('tabRestMinW')*60+getWheel('tabRestSecW');send({cmd:'tabata_cfg',work:ws||20,rest:rs||10,intervals:getWheel('tabIntW')||8,workColor:+document.getElementById('tabWC').value,restColor:+document.getElementById('tabRC').value});}
function toggleAnim(){send({cmd:'animtoggle',value:document.getElementById('animToggle').checked});}
function setTimezone(){sendSave({cmd:'timezone',value:+document.getElementById('tzSelect').value});}
function setDST(v){sendSave({cmd:'dst',value:v});}
function toggleMMSS(){send({cmd:'clockfmt',mmss:document.getElementById('mmssToggle').checked});}
function saveDSTRules(){sendSave({cmd:'dst_rules',dsFL:document.getElementById('dsFL').value==='1',dsDow:+document.getElementById('dsDow').value,dsMon:+document.getElementById('dsMon').value,dsH:+document.getElementById('dsHour').value,deFL:document.getElementById('deFL').value==='1',deDow:+document.getElementById('deDow').value,deMon:+document.getElementById('deMon').value,deH:+document.getElementById('deHour').value});}
function resetDSTIsrael(){sendSave({cmd:'dst_reset_israel'});}
function resetWifi(){if(confirm('Reset WiFi? Watch will restart.'))send({cmd:'resetwifi'});}
function applyCustomColor(){const h=document.getElementById('customColor').value;send({cmd:'customcolor',r:parseInt(h.substr(1,2),16),g:parseInt(h.substr(3,2),16),b:parseInt(h.substr(5,2),16)});}
function saveNightShift(){sendSave({cmd:'nightshift',enabled:document.getElementById('nsToggle').checked,start:+document.getElementById('nsStart').value,end:+document.getElementById('nsEnd').value,bright:+document.getElementById('nsBright').value});}
function pomToggle(bc){if(st.pomRun)send({cmd:'pom',action:'stop'});else send({cmd:'pom',action:'start',broadcast:!!bc});}
function savePomInt(){send({cmd:'pom_cfg',intervals:+document.getElementById('pomIntSlider').value});}
const CITIES=[['Jerusalem',31.77,35.22],['Tel Aviv',32.08,34.78],['Haifa',32.79,34.99],['Beer Sheva',31.25,34.79],['Rishon LeZion',31.96,34.80],['Petah Tikva',32.09,34.89],['Ashdod',31.80,34.65],['Netanya',32.33,34.86],['Holon',32.02,34.78],['Bnei Brak',32.09,34.83],['Ramat Gan',32.07,34.82],['Rehovot',31.90,34.81],['Ashkelon',31.67,34.57],['Bat Yam',32.02,34.75],['Herzliya',32.16,34.84],['Kfar Saba',32.18,34.91],['Hadera',32.44,34.92],['Modiin',31.90,35.01],['Nazareth',32.70,35.30],['Eilat',29.56,34.95],['Raanana',32.18,34.87],['Tiberias',32.79,35.53],['Acre',32.93,35.07],['Nahariya',33.01,35.10],['Kiryat Gat',31.61,34.76],['Afula',32.61,35.29],['Carmiel',32.91,35.30],['Arad',31.26,35.21]];
function initCities(){var s=document.getElementById('locSelect');var other=s.lastChild;CITIES.sort(function(a,b){return a[0].localeCompare(b[0]);}).forEach(function(c){var o=document.createElement('option');o.value=c[1].toFixed(2)+','+c[2].toFixed(2);o.textContent=c[0];s.insertBefore(o,other);});}
function setLoc(){var s=document.getElementById('locSelect'),v=s.value;document.getElementById('locManual').style.display=v==='other'?'flex':'none';if(v==='auto'){sendSave({cmd:'setloc',lat:0,lon:0,city:''});document.getElementById('locStatus').textContent='Using auto-detect';}else if(v!=='other'){var p=v.split(','),name=s.options[s.selectedIndex].textContent;sendSave({cmd:'setloc',lat:parseFloat(p[0]),lon:parseFloat(p[1]),city:name});document.getElementById('locStatus').textContent='Set: '+name;}}
function saveManualLoc(){var la=parseFloat(document.getElementById('locLat').value),lo=parseFloat(document.getElementById('locLon').value);if(la&&lo){sendSave({cmd:'setloc',lat:la,lon:lo});document.getElementById('locStatus').textContent='Saved ('+la.toFixed(2)+', '+lo.toFixed(2)+')';}}
function saveInfo(){sendSave({cmd:'datedisp',enabled:document.getElementById('dateToggle').checked,interval:+document.getElementById('dateIntSlider').value,tempEn:document.getElementById('tempToggle').checked,feelsLike:document.querySelector('input[name=tempType]:checked').value==='1',tempClr:document.getElementById('tempClrToggle').checked});}
function addBday(){var n=document.getElementById('bdayName').value,d=+document.getElementById('bdayDay').value,m=+document.getElementById('bdayMon').value;if(n&&d&&m)sendSave({cmd:'bday_add',name:n,day:d,month:m});document.getElementById('bdayName').value='';}
function delBday(i){sendSave({cmd:'bday_del',index:i});}
function loadTabPreset(){sendSave({cmd:'tab_preset_load',index:+document.getElementById('tabPresetSel').value});}
function saveTabPreset(){var n=document.getElementById('tabPresetName').value;if(!n)return;var ws=getWheel('tabWorkMinW')*60+getWheel('tabWorkSecW'),rs=getWheel('tabRestMinW')*60+getWheel('tabRestSecW');sendSave({cmd:'tab_preset_save',name:n,work:ws||20,rest:rs||10,intervals:getWheel('tabIntW')||8});}
function delTabPreset(){var i=+document.getElementById('tabPresetSel').value;sendSave({cmd:'tab_preset_del',index:i});}
function loadTabPresetIdx(i){sendSave({cmd:'tab_preset_load',index:i});}
function delTabPresetIdx(i){sendSave({cmd:'tab_preset_del',index:i});}
function startOTA(){
  var pass=document.getElementById('otaPass').value;
  if(pass!=='neotick2024'){alert('Wrong password');return;}
  var file=document.getElementById('otaFile').files[0];
  if(!file){alert('Select a .bin file first');return;}
  if(!file.name.endsWith('.bin')){alert('Only .bin files are allowed');return;}
  if(!confirm('Are you sure? The watch will restart after update.')){return;}
  document.getElementById('otaForm').style.display='none';
  document.getElementById('otaProgress').style.display='block';
  var xhr=new XMLHttpRequest();
  var form=new FormData();form.append('firmware',file);
  xhr.upload.onprogress=function(e){if(e.lengthComputable){var pct=Math.round(e.loaded/e.total*100);document.getElementById('otaBar').style.width=pct+'%';document.getElementById('otaPct').textContent=pct+'%';document.getElementById('otaStatus').textContent='Uploading... '+pct+'%';}};
  xhr.onload=function(){if(xhr.responseText==='FAIL'){document.getElementById('otaStatus').textContent='Update Failed!';document.getElementById('otaStatus').style.color='var(--danger)';document.getElementById('otaPct').textContent='The file may be corrupted. Please try again.';setTimeout(function(){document.getElementById('otaForm').style.display='block';document.getElementById('otaProgress').style.display='none';},3000);}else{document.getElementById('otaBar').style.width='100%';document.getElementById('otaStatus').textContent='Update Successful!';document.getElementById('otaStatus').style.color='var(--success)';var cd=5;document.getElementById('otaPct').textContent='Watch will restart in '+cd+'s. Do not disconnect power.';var ti=setInterval(function(){cd--;document.getElementById('otaPct').textContent='Watch will restart in '+cd+'s. Do not disconnect power.';if(cd<=0){clearInterval(ti);document.getElementById('otaPct').textContent='Restarting... Please wait and reconnect.';}},1000);}};
  xhr.onerror=function(){document.getElementById('otaStatus').textContent='Upload Error';document.getElementById('otaStatus').style.color='var(--danger)';document.getElementById('otaPct').textContent='Network error. Check connection.';};
  xhr.open('POST','/update?pass='+encodeURIComponent(pass));xhr.send(form);
}
init();
</script>
</body></html>
)=====";

// ======================== Stopwatch ========================
void WebUI::stopwatchStart() { if (!m_swRunning) { m_swStartTime = millis(); m_swRunning = true; } }  // resumes from accumulated
void WebUI::stopwatchRestart() { m_swAccumulated = 0; m_swStartTime = millis(); m_swRunning = true; }  // fresh start
void WebUI::stopwatchStop() { if (m_swRunning) { m_swAccumulated += millis() - m_swStartTime; m_swRunning = false; } }
void WebUI::stopwatchReset() { m_swRunning = false; m_swAccumulated = 0; m_swStartTime = 0; }
unsigned long WebUI::getStopwatchElapsed() const { return m_swRunning ? m_swAccumulated + (millis() - m_swStartTime) : m_swAccumulated; }

// ======================== Timer ========================
void WebUI::timerSet(unsigned long durationMs) { m_timerDuration = durationMs; m_timerRemaining = durationMs; m_timerDone = false; }
void WebUI::timerStart() { if (!m_timerRunning && m_timerRemaining > 0) { m_timerStartTime = millis(); m_timerRunning = true; m_timerDone = false; } }
void WebUI::timerStop() { if (m_timerRunning) { unsigned long e = millis() - m_timerStartTime; m_timerRemaining = (e >= m_timerRemaining) ? 0 : m_timerRemaining - e; m_timerRunning = false; } }
void WebUI::timerReset() { m_timerRunning = false; m_timerRemaining = m_timerDuration; m_timerDone = false; }
long WebUI::getTimerRemaining() const { if (m_timerRunning) { unsigned long e = millis() - m_timerStartTime; return (e >= m_timerRemaining) ? 0 : (long)(m_timerRemaining - e); } return (long)m_timerRemaining; }

// ======================== Tabata ========================
void WebUI::tabataStart() {
    if (m_tabDone) tabataReset();
    if (!m_tabRunning) {
        m_tabRunning = true; m_tabWorkPhase = true;
        if (m_tabCurrentInterval < 1) m_tabCurrentInterval = 1;
        m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000;
        m_tabPhaseStart = millis();
    }
}
void WebUI::tabataStop() { if (m_tabRunning) { unsigned long e = millis() - m_tabPhaseStart; m_tabPhaseDuration = (e >= m_tabPhaseDuration) ? 0 : m_tabPhaseDuration - e; m_tabRunning = false; } }
void WebUI::tabataReset() { m_tabRunning = false; m_tabDone = false; m_tabWorkPhase = true; m_tabCurrentInterval = 1; m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000; m_tabPhaseStart = 0; }
long WebUI::getTabataPhaseRemaining() const { if (!m_tabRunning) return (long)m_tabPhaseDuration; unsigned long e = millis() - m_tabPhaseStart; return (e >= m_tabPhaseDuration) ? 0 : (long)(m_tabPhaseDuration - e); }
void WebUI::tabataAdvance() {
    if (!m_tabRunning || getTabataPhaseRemaining() > 0) return;
    m_tabPhaseChanged = true;  // signal main.cpp to buzz
    if (m_tabWorkPhase) {
        m_tabWorkPhase = false; m_tabPhaseDuration = (unsigned long)m_settings->tabata.restSec * 1000; m_tabPhaseStart = millis();
    } else {
        m_tabCurrentInterval++;
        if (m_tabCurrentInterval > m_settings->tabata.intervals) { m_tabRunning = false; m_tabDone = true; m_tabPhaseDuration = 0; }
        else { m_tabWorkPhase = true; m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000; m_tabPhaseStart = millis(); }
    }
}

// ======================== Pomodoro ========================
void WebUI::pomodoroStart() {
    if (m_pomDone) pomodoroReset();
    if (!m_pomRunning) {
        m_pomRunning = true; m_pomWorkPhase = true;
        if (m_pomCurrentInterval < 1) m_pomCurrentInterval = 1;
        m_pomPhaseDuration = (unsigned long)POMODORO_WORK_SEC * 1000;
        m_pomPhaseStart = millis();
    }
}
void WebUI::pomodoroStop() {
    if (m_pomRunning) { unsigned long e = millis() - m_pomPhaseStart; m_pomPhaseDuration = (e >= m_pomPhaseDuration) ? 0 : m_pomPhaseDuration - e; m_pomRunning = false; }
}
void WebUI::pomodoroReset() {
    m_pomRunning = false; m_pomDone = false; m_pomWorkPhase = true; m_pomCurrentInterval = 1;
    m_pomPhaseDuration = (unsigned long)POMODORO_WORK_SEC * 1000; m_pomPhaseStart = 0;
}
long WebUI::getPomodoroPhaseRemaining() const {
    if (!m_pomRunning) return (long)m_pomPhaseDuration;
    unsigned long e = millis() - m_pomPhaseStart;
    return (e >= m_pomPhaseDuration) ? 0 : (long)(m_pomPhaseDuration - e);
}
void WebUI::pomodoroAdvance() {
    if (!m_pomRunning || getPomodoroPhaseRemaining() > 0) return;
    if (m_pomWorkPhase) {
        m_pomWorkPhase = false; m_pomPhaseDuration = (unsigned long)POMODORO_BREAK_SEC * 1000; m_pomPhaseStart = millis();
    } else {
        m_pomCurrentInterval++;
        if (m_pomCurrentInterval > m_settings->pomodoroIntervals) { m_pomRunning = false; m_pomDone = true; m_pomPhaseDuration = 0; }
        else { m_pomWorkPhase = true; m_pomPhaseDuration = (unsigned long)POMODORO_WORK_SEC * 1000; m_pomPhaseStart = millis(); }
    }
}

// ======================== JSON Helpers ========================
static int extractInt(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return -9999;
    p = json.indexOf(':', p); if (p < 0) return -9999; p++;
    while (p < (int)json.length() && (json[p] == ' ' || json[p] == '"')) p++;
    bool neg = false; if (p < (int)json.length() && json[p] == '-') { neg = true; p++; }
    String n; while (p < (int)json.length() && json[p] >= '0' && json[p] <= '9') n += json[p++];
    if (n.length() == 0) return -9999; int v = n.toInt(); return neg ? -v : v;
}
static String extractString(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return "";
    int a = json.indexOf('"', p + s.length() + 1), b = json.indexOf('"', a + 1);
    return (a < 0 || b < 0) ? "" : json.substring(a + 1, b);
}
static bool extractBool(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return false;
    int c = json.indexOf(',', p), e = json.indexOf('}', p);
    int end = (c >= 0 && c < e) ? c : e;
    return json.substring(p, end).indexOf("true") >= 0;
}
static float extractFloat(const String& json, const char* key) {
    String s = String("\"") + key + "\""; int p = json.indexOf(s); if (p < 0) return 0;
    p = json.indexOf(':', p); if (p < 0) return 0; p++;
    while (p < (int)json.length() && json[p] == ' ') p++;
    String n; while (p < (int)json.length() && (json[p] == '-' || json[p] == '.' || (json[p] >= '0' && json[p] <= '9'))) n += json[p++];
    return n.toFloat();
}

// ======================== WebSocket Handler ========================
void WebUI::handleWebSocketMessage(AsyncWebSocketClient* client, uint8_t* data, size_t len) {
    String msg; msg.reserve(len);
    for (size_t i = 0; i < len; i++) msg += (char)data[i];
    int cs = msg.indexOf("\"cmd\""); if (cs < 0) return;
    int vs = msg.indexOf('"', cs + 6), ve = msg.indexOf('"', vs + 1);
    if (vs < 0 || ve < 0) return;
    String cmd = msg.substring(vs + 1, ve);

    if (cmd == "color") { int i = extractInt(msg, "index"); if (i >= 0 && i < m_display->getColorCount()) { m_display->setColorByIndex(i); m_settings->colorIndex = i; m_pendingSave = millis(); } }
    else if (cmd == "customcolor") { int r = extractInt(msg, "r"), g = extractInt(msg, "g"), b = extractInt(msg, "b"); if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) { m_display->setColor(CRGB(r, g, b)); m_settings->colorIndex = -1; m_settings->customR = r; m_settings->customG = g; m_settings->customB = b; m_pendingSave = millis(); } }
    else if (cmd == "brightness") { int v = extractInt(msg, "value"); if (v >= 0 && v <= MAX_BRIGHTNESS) { m_display->setBrightness(v); m_settings->brightness = v; m_pendingSave = millis(); } }
    else if (cmd == "mode") { int v = extractInt(msg, "value"); if (v >= 0 && v <= 4) m_mode = (DisplayMode)v; }
    else if (cmd == "clockfmt") { m_settings->clockShowMMSS = extractBool(msg, "mmss"); m_pendingSave = millis(); }
    else if (cmd == "sw") { String a = extractString(msg, "action"); if (a == "start") { stopwatchStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_STOPWATCH, true, 0, true, 1, 1, false); } else if (a == "restart") { stopwatchRestart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_STOPWATCH, true, 0, true, 1, 1, false); } else if (a == "stop") { stopwatchStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { stopwatchReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "timer") { String a = extractString(msg, "action"); if (a == "start") { int d = extractInt(msg, "duration"); if (d > 0) timerSet(d); timerStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_TIMER, true, getTimerRemaining(), true, 1, 1, false); } else if (a == "set") { int d = extractInt(msg, "duration"); if (d > 0) timerSet(d); } else if (a == "stop") { timerStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { timerReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "tabata") { String a = extractString(msg, "action"); if (a == "start") { tabataStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_TABATA, true, getTabataPhaseRemaining(), true, 1, m_settings->tabata.intervals, false); } else if (a == "stop") { tabataStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { tabataReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "tabata_cfg") { int w = extractInt(msg, "work"), r = extractInt(msg, "rest"), n = extractInt(msg, "intervals"), wc = extractInt(msg, "workColor"), rc = extractInt(msg, "restColor"); if (w > 0) m_settings->tabata.workSec = w; if (r > 0) m_settings->tabata.restSec = r; if (n > 0) m_settings->tabata.intervals = n; if (wc >= 0) m_settings->tabata.workColorIdx = wc; if (rc >= 0) m_settings->tabata.restColorIdx = rc; m_pendingSave = millis(); tabataReset(); }
    else if (cmd == "timezone") { long v = (long)extractInt(msg, "value"); m_timeMgr->setTimezoneOffset(v); m_settings->timezoneOffset = v; m_pendingSave = millis(); }
    else if (cmd == "dst") { int v = extractInt(msg, "value"); m_timeMgr->setDSTMode(v); m_settings->dstMode = v; m_pendingSave = millis(); }
    else if (cmd == "dst_rules") { DSTRule s, e; s.isLast = extractBool(msg, "dsFL"); s.dayOfWeek = extractInt(msg, "dsDow"); s.month = extractInt(msg, "dsMon"); s.hour = extractInt(msg, "dsH"); e.isLast = extractBool(msg, "deFL"); e.dayOfWeek = extractInt(msg, "deDow"); e.month = extractInt(msg, "deMon"); e.hour = extractInt(msg, "deH"); m_settings->dstStart = s; m_settings->dstEnd = e; m_timeMgr->setDSTRules(s, e); m_pendingSave = millis(); }
    else if (cmd == "dst_reset_israel") { m_settings->dstStart = DST_ISRAEL_START; m_settings->dstEnd = DST_ISRAEL_END; m_timeMgr->resetDSTToIsrael(); m_pendingSave = millis(); }
    else if (cmd == "animate") { m_animationRequested = true; }
    else if (cmd == "colon") { m_settings->colonLedsEnabled = extractBool(msg, "enabled"); m_pendingSave = millis(); }
    else if (cmd == "colormode") { int v = extractInt(msg, "value"); if (v >= 0 && v <= 3) { m_settings->colorMode = v; m_pendingSave = millis(); } }
    else if (cmd == "animtoggle") { m_settings->animateTransitions = extractBool(msg, "value"); m_pendingSave = millis(); }
    else if (cmd == "nightshift") { m_settings->nightShiftEnabled = extractBool(msg, "enabled"); int sh = extractInt(msg, "start"), eh = extractInt(msg, "end"), nb = extractInt(msg, "bright"); if (sh >= 0 && sh <= 23) m_settings->nightShiftStartHour = sh; if (eh >= 0 && eh <= 23) m_settings->nightShiftEndHour = eh; if (nb >= 0 && nb <= MAX_BRIGHTNESS) m_settings->nightShiftBrightness = nb; m_pendingSave = millis(); }
    else if (cmd == "pom") { String a = extractString(msg, "action"); if (a == "start") { pomodoroStart(); if (extractBool(msg, "broadcast") && m_heartbeat) m_heartbeat->startBroadcast(MODE_POMODORO, true, getPomodoroPhaseRemaining(), true, 1, m_settings->pomodoroIntervals, false); } else if (a == "stop") { pomodoroStop(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } else if (a == "reset") { pomodoroReset(); if (m_heartbeat) m_heartbeat->stopBroadcast(); } }
    else if (cmd == "pom_cfg") { int n = extractInt(msg, "intervals"); if (n > 0 && n <= 8) { m_settings->pomodoroIntervals = n; m_pendingSave = millis(); } }
    else if (cmd == "datedisp") { m_settings->showDateEnabled = extractBool(msg, "enabled"); m_settings->showTempEnabled = extractBool(msg, "tempEn"); m_settings->tempFeelsLike = extractBool(msg, "feelsLike"); m_settings->tempColorByValue = extractBool(msg, "tempClr"); int iv = extractInt(msg, "interval"); if (iv > 0) m_settings->showDateIntervalSec = iv; m_pendingSave = millis(); }
    else if (cmd == "setloc") { m_settings->weatherLat = extractFloat(msg, "lat"); m_settings->weatherLon = extractFloat(msg, "lon"); String cn = extractString(msg, "city"); extern char weatherCity[32]; if (cn.length()) { strncpy(weatherCity, cn.c_str(), 31); weatherCity[31] = 0; } else if (m_settings->weatherLat == 0) { weatherCity[0] = 0; } m_pendingSave = millis(); extern volatile bool weatherFetchNow; weatherFetchNow = true; }
    else if (cmd == "buzzer") { int lv = extractInt(msg, "level"); if (lv >= 0 && lv <= 2) { m_settings->buzzerLevel = lv; m_pendingSave = millis(); } }
    else if (cmd == "buzztest") { m_buzzerTestRequested = true; }
    else if (cmd == "clockwork") { m_settings->clockworkBuzzer = extractBool(msg, "enabled"); m_pendingSave = millis(); }

    else if (cmd == "bday_add") { int idx = m_settings->birthdayCount; if (idx < MAX_BIRTHDAYS) { Birthday b; String n = extractString(msg, "name"); strncpy(b.name, n.c_str(), 15); b.name[15] = 0; b.day = extractInt(msg, "day"); b.month = extractInt(msg, "month"); m_configStore->saveBirthday(idx, b); m_settings->birthdayCount = idx + 1; Preferences p; p.begin(PREFS_NS, false); p.putUChar("bdCnt", m_settings->birthdayCount); p.end(); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "bday_del") { int i = extractInt(msg, "index"); if (i >= 0 && i < m_settings->birthdayCount) { for (int j = i; j < m_settings->birthdayCount - 1; j++) { Birthday b; m_configStore->loadBirthday(j + 1, b); m_configStore->saveBirthday(j, b); } m_settings->birthdayCount--; Birthday empty; m_configStore->saveBirthday(m_settings->birthdayCount, empty); Preferences p; p.begin(PREFS_NS, false); p.putUChar("bdCnt", m_settings->birthdayCount); p.end(); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "tab_preset_save") { String n = extractString(msg, "name"); int w = extractInt(msg, "work"), r = extractInt(msg, "rest"), iv = extractInt(msg, "intervals"); TabataPreset p; strncpy(p.name, n.c_str(), 15); p.name[15] = 0; p.workSec = w; p.restSec = r; p.intervals = iv; for (int i = 0; i < MAX_TABATA_PRESETS; i++) { TabataPreset ex; m_configStore->loadTabataPreset(i, ex); if (ex.name[0] == 0) { m_configStore->saveTabataPreset(i, p); break; } } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "tab_preset_del") { int i = extractInt(msg, "index"); if (i >= 0 && i < MAX_TABATA_PRESETS) { TabataPreset empty; m_configStore->saveTabataPreset(i, empty); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "tab_preset_load") { int i = extractInt(msg, "index"); TabataPreset p; m_configStore->loadTabataPreset(i, p); if (p.name[0]) { m_settings->tabata.workSec = p.workSec; m_settings->tabata.restSec = p.restSec; m_settings->tabata.intervals = p.intervals; m_pendingSave = millis(); tabataReset(); } if (m_ws) m_ws->textAll(buildStateJSON()); return; }
    else if (cmd == "resetwifi") { Preferences p; p.begin(NVS_NAMESPACE, false); p.remove("ssid"); p.remove("pass"); p.end(); delay(500); ESP.restart(); }
    if (m_ws) m_ws->textAll(buildStateJSON());
}

// ======================== State Broadcast ========================
// Fast state: only dynamic values that change every second (~300 bytes)
String WebUI::buildFastJSON() {
    String j = "{\"h\":"; j += m_timeMgr->getHours();
    j += ",\"m\":"; j += m_timeMgr->getMinutes();
    j += ",\"s\":"; j += m_timeMgr->getSeconds();
    j += ",\"dv\":"; j += m_displayValue;
    j += ",\"dt\":"; j += m_displayTemp ? "true" : "false";
    j += ",\"db\":"; j += m_displayBlank ? "true" : "false";
    j += ",\"mode\":"; j += (int)m_mode;
    CRGB c = m_display->activeColor();
    char hex[8]; snprintf(hex, sizeof(hex), "#%02X%02X%02X", c.r, c.g, c.b);
    j += ",\"clr\":\""; j += hex; j += "\"";
    j += ",\"swMs\":"; j += getStopwatchElapsed();
    j += ",\"swRun\":"; j += m_swRunning ? "true" : "false";
    j += ",\"tmMs\":"; j += getTimerRemaining();
    j += ",\"tmRun\":"; j += m_timerRunning ? "true" : "false";
    j += ",\"tmDone\":"; j += m_timerDone ? "true" : "false";
    j += ",\"tmDur\":"; j += m_timerDuration;
    j += ",\"tabMs\":"; j += getTabataPhaseRemaining();
    j += ",\"tabRun\":"; j += m_tabRunning ? "true" : "false";
    j += ",\"tabWork\":"; j += m_tabWorkPhase ? "true" : "false";
    j += ",\"tabInt\":"; j += m_tabCurrentInterval;
    j += ",\"tabDone\":"; j += m_tabDone ? "true" : "false";
    j += ",\"tabPaused\":"; j += (!m_tabRunning && !m_tabDone && m_tabPhaseStart > 0) ? "true" : "false";
    j += ",\"pomMs\":"; j += getPomodoroPhaseRemaining();
    j += ",\"pomRun\":"; j += m_pomRunning ? "true" : "false";
    j += ",\"pomWork\":"; j += m_pomWorkPhase ? "true" : "false";
    j += ",\"pomInt\":"; j += m_pomCurrentInterval;
    j += ",\"pomDone\":"; j += m_pomDone ? "true" : "false";
    j += ",\"anim\":"; j += m_animating ? "true" : "false";
    j += ",\"synced\":"; j += m_timeMgr->isTimeSynced() ? "true" : "false";
    j += ",\"wifiLost\":"; j += (WiFi.status() != WL_CONNECTED) ? "true" : "false";
    j += ",\"clrMode\":"; j += m_settings->colorMode;
    int cpuTemp = (int)temperatureRead();
    j += ",\"temp\":"; j += cpuTemp;
    if (cpuTemp >= THERMAL_THROTTLE_TEMP) { j += ",\"thermThrot\":true"; }
    if (m_heartbeat) {
        j += ",\"peers\":"; j += m_heartbeat->getPeerCount();
        if (m_heartbeat->isBroadcasting()) j += ",\"bcasting\":true";
    }
    j += "}";
    return j;
}

// Full state: everything including settings (sent on connect + after settings change)
String WebUI::buildStateJSON() {
    String j = buildFastJSON();
    // Remove closing brace and append settings
    j.remove(j.length() - 1);
    j += ",\"full\":true";
    j += ",\"colorIdx\":"; j += m_display->getColorIndex();
    j += ",\"bright\":"; j += m_settings->brightness;
    j += ",\"mmss\":"; j += m_settings->clockShowMMSS ? "true" : "false";
    j += ",\"tabTotal\":"; j += m_settings->tabata.intervals;
    j += ",\"tz\":"; j += m_timeMgr->getTimezoneOffset();
    j += ",\"dst\":"; j += m_timeMgr->getDSTMode();
    j += ",\"dsFL\":"; j += m_settings->dstStart.isLast ? "true" : "false";
    j += ",\"dsDow\":"; j += m_settings->dstStart.dayOfWeek;
    j += ",\"dsMon\":"; j += m_settings->dstStart.month;
    j += ",\"dsH\":"; j += m_settings->dstStart.hour;
    j += ",\"deFL\":"; j += m_settings->dstEnd.isLast ? "true" : "false";
    j += ",\"deDow\":"; j += m_settings->dstEnd.dayOfWeek;
    j += ",\"deMon\":"; j += m_settings->dstEnd.month;
    j += ",\"deH\":"; j += m_settings->dstEnd.hour;
    j += ",\"tbWork\":"; j += m_settings->tabata.workSec;
    j += ",\"tbRest\":"; j += m_settings->tabata.restSec;
    j += ",\"tbInt2\":"; j += m_settings->tabata.intervals;
    j += ",\"tbWC\":"; j += m_settings->tabata.workColorIdx;
    j += ",\"tbRC\":"; j += m_settings->tabata.restColorIdx;
    j += ",\"animTr\":"; j += m_settings->animateTransitions ? "true" : "false";
    j += ",\"nsEn\":"; j += m_settings->nightShiftEnabled ? "true" : "false";
    j += ",\"nsStart\":"; j += m_settings->nightShiftStartHour;
    j += ",\"nsEnd\":"; j += m_settings->nightShiftEndHour;
    j += ",\"nsBright\":"; j += m_settings->nightShiftBrightness;
    j += ",\"pomTotal\":"; j += m_settings->pomodoroIntervals;
    j += ",\"dateEn\":"; j += m_settings->showDateEnabled ? "true" : "false";
    j += ",\"dateInt\":"; j += m_settings->showDateIntervalSec;
    j += ",\"tempEn\":"; j += m_settings->showTempEnabled ? "true" : "false";
    j += ",\"tempFL\":"; j += m_settings->tempFeelsLike ? "true" : "false";
    j += ",\"tempClr\":"; j += m_settings->tempColorByValue ? "true" : "false";
    if (!isnan(m_currentTemp)) { j += ",\"curTemp\":"; j += String(m_currentTemp, 1); j += ",\"curFL\":"; j += String(m_currentFeelsLike, 1); }
    else { j += ",\"curTemp\":null,\"curFL\":null"; }
    if (m_weatherCity.length() > 0) { j += ",\"wLoc\":\""; j += m_weatherCity; j += "\""; }
    if (m_settings->weatherLat != 0) { j += ",\"wLat\":"; j += String(m_settings->weatherLat, 2); j += ",\"wLon\":"; j += String(m_settings->weatherLon, 2); }
    j += ",\"colonEn\":"; j += m_settings->colonLedsEnabled ? "true" : "false";
    j += ",\"buzzLv\":"; j += m_settings->buzzerLevel;
    j += ",\"cwBuzz\":"; j += m_settings->clockworkBuzzer ? "true" : "false";

    j += ",\"rssi\":"; j += WiFi.RSSI();
    j += ",\"bdays\":[";
    for (int i = 0; i < m_settings->birthdayCount && i < MAX_BIRTHDAYS; i++) {
        Birthday b; m_configStore->loadBirthday(i, b);
        if (i > 0) j += ",";
        j += "{\"n\":\""; j += b.name; j += "\",\"d\":"; j += b.day; j += ",\"m\":"; j += b.month; j += "}";
    }
    j += "]";
    j += ",\"tabPresets\":[";
    for (int i = 0; i < MAX_TABATA_PRESETS; i++) {
        TabataPreset p; m_configStore->loadTabataPreset(i, p);
        if (i > 0) j += ",";
        j += "{\"n\":\""; j += p.name; j += "\",\"w\":"; j += p.workSec; j += ",\"r\":"; j += p.restSec; j += ",\"i\":"; j += p.intervals; j += "}";
    }
    j += "]";
    j += ",\"ssid\":\""; j += WiFi.SSID(); j += "\"";
    j += ",\"ip\":\""; j += WiFi.localIP().toString(); j += "\"";
    j += "}";
    return j;
}

// Regular broadcast: fast JSON only. Full state sent on connect + after commands.
void WebUI::broadcastState() { if (!m_ws || m_ws->count() == 0) return; m_ws->textAll(buildFastJSON()); }

// ======================== Hub Picker ========================
String WebUI::buildHubHTML() {
    String h = F("<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>NeoTick Hub</title>"
    "<style>:root{--bg:#0f0f23;--card:#1a1a2e;--accent:#44d9e1;--text:#e0e0e0;--text2:#999}"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:var(--bg);color:var(--text);min-height:100vh;padding:20px}"
    ".hdr{text-align:center;padding:30px 0 20px}.hdr h1{font-size:28px;color:var(--accent);font-weight:800}.hdr p{color:var(--text2);font-size:14px;margin-top:4px}"
    ".grid{max-width:500px;margin:0 auto;display:flex;flex-direction:column;gap:12px}"
    ".dev{background:var(--card);border-radius:14px;padding:20px 22px;text-decoration:none;color:var(--text);display:flex;align-items:center;gap:16px;border:2px solid transparent;transition:.15s}"
    ".dev:hover,.dev:active{border-color:var(--accent);transform:scale(1.02)}"
    ".dot{width:12px;height:12px;border-radius:50%;background:var(--accent);flex-shrink:0}"
    ".info{flex:1}.name{font-size:17px;font-weight:700}.ip{font-size:13px;color:var(--text2);margin-top:2px}"
    ".arr{color:var(--accent);font-size:20px}"
    "</style></head><body>"
    "<div class='hdr'><h1>NeoTick</h1><p>Select a watch to control</p></div><div class='grid'>");

    // Add self
    h += "<a class='dev' href='http://";
    h += WiFi.localIP().toString();
    h += "/'><span class='dot'></span><div class='info'><div class='name'>";
    h += m_heartbeat->getDeviceName();
    h += "</div><div class='ip'>";
    h += WiFi.localIP().toString();
    h += " (this watch)</div></div><span class='arr'>&#9654;</span></a>";

    // Add peers
    for (int i = 0; i < m_heartbeat->getPeerCount(); i++) {
        const PeerInfo& p = m_heartbeat->getPeer(i);
        h += "<a class='dev' href='http://";
        h += p.ip;
        h += "/'><span class='dot'></span><div class='info'><div class='name'>";
        h += p.name.length() > 0 ? p.name : "NeoTick";
        h += "</div><div class='ip'>";
        h += p.ip;
        h += "</div></div><span class='arr'>&#9654;</span></a>";
    }

    h += "</div></body></html>";
    return h;
}

// ======================== Setup ========================
void WebUI::begin(LedDisplay* display, TimeManager* timeMgr, ConfigStore* configStore, WatchSettings* settings, WifiManager* wifiMgr, Heartbeat* heartbeat) {
    m_display = display; m_timeMgr = timeMgr; m_configStore = configStore; m_settings = settings; m_wifiMgr = wifiMgr; m_heartbeat = heartbeat;
    m_tabPhaseDuration = (unsigned long)m_settings->tabata.workSec * 1000;
    m_server = new AsyncWebServer(WEB_PORT);
    m_ws = new AsyncWebSocket(WS_PATH);
    m_ws->onEvent([this](AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void*, uint8_t* data, size_t len) {
        if (type == WS_EVT_CONNECT) {
            Serial.printf("WS #%u connected\n", client->id());
            client->text(buildStateJSON());
        }
        else if (type == WS_EVT_DISCONNECT) {
            Serial.printf("WS #%u disconnected\n", client->id());
        }
        else if (type == WS_EVT_DATA) { handleWebSocketMessage(client, data, len); }
    });
    m_server->addHandler(m_ws);

    // Hub picker: if accessed via neotick.local and peers exist, show device picker
    // Otherwise serve the normal watch UI
    m_server->on("/", HTTP_GET, [this](AsyncWebServerRequest* r) {
        bool viaLocal = false;
        if (r->hasHeader("Host")) {
            String host = r->header("Host");
            viaLocal = host.startsWith("neotick.local");
        }
        if (viaLocal && m_heartbeat && m_heartbeat->getPeerCount() > 0) {
            // Serve hub picker page
            String html = buildHubHTML();
            r->send(200, "text/html", html);
        } else {
            // Serve normal watch UI
            size_t htmlLen = strlen_P(WEB_HTML);
            AsyncWebServerResponse* resp = r->beginChunkedResponse("text/html",
                [htmlLen](uint8_t* buffer, size_t maxLen, size_t index) -> size_t {
                    if (index >= htmlLen) return 0;
                    size_t len = std::min(maxLen, htmlLen - index);
                    memcpy_P((char*)buffer, WEB_HTML + index, len);
                    return len;
                });
            r->send(resp);
        }
    });
    extern const char* getLogBuffer();
    m_server->on("/logs", HTTP_GET, [](AsyncWebServerRequest* r) { r->send(200, "text/plain", getLogBuffer()); });
    m_server->on("/update", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(200, "text/html", "<html><body style='background:#0f0f23;color:#e0e0e0;font-family:sans-serif;text-align:center;padding:40px'><h2>Firmware Update</h2><p style='color:#e74c3c'>Developer Only - Use main UI for guided update</p><form id='f' method='POST' enctype='multipart/form-data'><input type='password' id='p' placeholder='Password' style='margin:10px;padding:8px'><br><input type='file' name='firmware' style='margin:10px'><br><input type='button' value='Upload' onclick=\"f.action='/update?pass='+encodeURIComponent(p.value);f.submit()\" style='padding:12px 24px;font-size:16px;cursor:pointer'></form></body></html>");
    });
    m_server->on("/update", HTTP_POST,
        [](AsyncWebServerRequest* r) {
            bool ok = !Update.hasError() && otaMagicFound && otaPasswordOK;
            r->send(200, "text/plain", ok ? "OK" : "FAIL");
            if (ok) { delay(500); ESP.restart(); }
        },
        [](AsyncWebServerRequest* r, String filename, size_t index, uint8_t* data, size_t len, bool final) {
            if (!index) {
                // Check password from query param
                otaPasswordOK = r->hasParam("pass") && r->getParam("pass")->value() == OTA_PASSWORD;
                otaMagicFound = false;
                otaTailLen = 0;
                if (!otaPasswordOK) return;
                Update.begin(UPDATE_SIZE_UNKNOWN);
            }
            if (!otaPasswordOK) return;

            // Search for magic marker in uploaded data
            if (!otaMagicFound) {
                // Check overlap from previous chunk boundary
                if (otaTailLen > 0 && len > 0) {
                    uint8_t buf[OTA_MAGIC_LEN * 2];
                    memcpy(buf, otaTail, otaTailLen);
                    size_t copyLen = min(len, (size_t)(OTA_MAGIC_LEN - 1));
                    memcpy(buf + otaTailLen, data, copyLen);
                    size_t searchLen = otaTailLen + copyLen;
                    for (size_t i = 0; i + OTA_MAGIC_LEN <= searchLen; i++) {
                        if (memcmp(buf + i, OTA_MAGIC, OTA_MAGIC_LEN) == 0) { otaMagicFound = true; break; }
                    }
                }
                // Search current chunk
                if (!otaMagicFound) {
                    for (size_t i = 0; i + OTA_MAGIC_LEN <= len; i++) {
                        if (memcmp(data + i, OTA_MAGIC, OTA_MAGIC_LEN) == 0) { otaMagicFound = true; break; }
                    }
                }
                // Save tail for cross-boundary check
                otaTailLen = min(len, (size_t)(OTA_MAGIC_LEN - 1));
                memcpy(otaTail, data + len - otaTailLen, otaTailLen);
            }

            Update.write(data, len);
            if (final) {
                if (otaMagicFound) Update.end(true);
                else Update.abort();
            }
        }
    );
    m_server->begin();
    Serial.println("Web UI started on port 80");
}

void WebUI::update() {
    tabataAdvance();
    pomodoroAdvance();
    if (m_timerRunning && getTimerRemaining() <= 0) { m_timerRunning = false; m_timerRemaining = 0; m_timerDone = true; }

    // Deferred NVS save: batch all changes, write once 2 seconds after last change
    unsigned long now = millis();
    if (m_pendingSave > 0 && now - m_pendingSave >= NVS_SAVE_DELAY_MS) {
        m_pendingSave = 0;
        m_configStore->save(*m_settings);  // single NVS write for all settings
    }

    unsigned long iv = (m_swRunning || m_tabRunning) ? WS_BROADCAST_FAST_MS : WS_BROADCAST_SLOW_MS;
    if (now - m_lastBroadcast >= iv) { m_lastBroadcast = now; broadcastState(); m_ws->cleanupClients(); }
    if (now - m_lastFullBroadcast >= WS_FULL_BROADCAST_MS) { m_lastFullBroadcast = now; if (m_ws && m_ws->count() > 0) m_ws->textAll(buildStateJSON()); }
}
