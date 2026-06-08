#ifndef CONFIG_H
#define CONFIG_H

// === Pin Configuration ===
#define LED_DATA_PIN        4
#define COLON_LED_PIN       23    // two "dumb" LEDs for seconds/colon indicator
#define BUZZER_PIN          25
#define BUZZER_LEDC_CH      0
#define NUM_DIGITS          4
#define NUM_LEDS_PER_DIGIT  29
#define TOTAL_LEDS          (NUM_DIGITS * NUM_LEDS_PER_DIGIT)

// LED 16 on each digit is a wiring-only LED — must never be lit
#define WIRING_ONLY_LED     16

// === LED Defaults ===
#define DEFAULT_BRIGHTNESS  100
#define MAX_BRIGHTNESS      230

// === WiFi ===
#define AP_SSID             "NeoTick"
#define WIFI_CONNECT_TIMEOUT_MS  10000
#define WIFI_RECONNECT_INTERVAL  30000

// === NTP ===
#define NTP_SERVER          "pool.ntp.org"
#define NTP_SYNC_INTERVAL   1800000  // 30 minutes
#define DST_CHECK_INTERVAL  300000   // 5 minutes

// === Web Server ===
#define WEB_PORT            80
#define WS_PATH             "/ws"
#define WS_BROADCAST_FAST_MS   200   // active mode (stopwatch/tabata)
#define WS_BROADCAST_SLOW_MS   500   // idle mode
#define WS_FULL_BROADCAST_MS   30000 // periodic full state sync
#define NVS_SAVE_DELAY_MS      2000  // debounce for settings writes

// === Heartbeat / Peer Discovery ===
#define MDNS_SERVICE_NAME    "neotick"
// Well-known universal entry point: one elected clock (lowest MAC) also answers this
// name so an instructor can always reach the clock list at http://neotick.local
#define GATEWAY_MDNS_NAME    "neotick"
#define MULTICAST_IP         "239.1.2.3"
#define MULTICAST_PORT       9999
#define HEARTBEAT_INTERVAL   2000    // ms
#define PEER_TIMEOUT         6000    // ms — evict after this
#define MAX_PEERS            8

// === NVS Keys ===
#define NVS_NAMESPACE       "watchcfg"

// === Config Sync ===
// Shared secret required on /applyconfig pushes. Not real security (it ships in the
// firmware), just a guard so a stray device can't trivially overwrite a watch's settings.
#define SYNC_TOKEN          "neotick-cfg-7a3f91"

// === Display Modes ===
enum DisplayMode {
    MODE_CLOCK = 0,
    MODE_STOPWATCH,
    MODE_TIMER,
    MODE_TABATA,
    MODE_POMODORO
};

// === Pomodoro Defaults ===
#define POMODORO_WORK_SEC    (25*60)
#define POMODORO_BREAK_SEC   (5*60)
#define POMODORO_INTERVALS   4

// === Max Birthdays / Presets ===
#define MAX_BIRTHDAYS        10
#define MAX_TABATA_PRESETS   5

// === DST Rule ===
struct DSTRule {
    bool isLast;
    int  dayOfWeek;
    int  month;
    int  hour;
};

#define DST_ISRAEL_START  { true, 5, 3, 2 }
#define DST_ISRAEL_END    { true, 0, 10, 2 }

// === Tabata Defaults ===
#define TABATA_DEFAULT_WORK_SEC   20
#define TABATA_DEFAULT_REST_SEC   10
#define TABATA_DEFAULT_INTERVALS  8

// === Animation Timing ===
#define STARTUP_LED_DELAY_MS    15
#define STARTUP_DIGIT_DELAY_MS  150
#define TRANSITION_STEP_MS      8

// === Colon Blink Rates ===
#define COLON_BLINK_NORMAL_MS   1000  // 0.5Hz gentle blink when WiFi OK
#define COLON_BLINK_FAST_MS     150   // ~3Hz when WiFi lost

// === Pause Pulse ===
#define PAUSE_PULSE_MIN_BRIGHT  15
#define PAUSE_PULSE_SPEED_MS    30    // ms per brightness step

// === Weather ===
#define WEATHER_FETCH_INTERVAL  600000   // 10 minutes

// === Thermal Protection ===
#define THERMAL_WARN_TEMP       60    // show warning in GUI
#define THERMAL_THROTTLE_TEMP   65    // auto-reduce brightness
#define THERMAL_THROTTLE_BRIGHT 80    // brightness cap when throttling

#endif // CONFIG_H
