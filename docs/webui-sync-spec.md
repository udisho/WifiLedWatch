# NeoTick WebUI Sync Spec

## Architecture

The WebUI communicates with the ESP32 over a single WebSocket connection.
Two JSON message types flow from server to client:

| Type | Marker | Sent when | Contains |
|------|--------|-----------|----------|
| **Fast** | no `full` field | Periodic broadcast (~200ms) | Display data only: time, timers, stopwatch, pomodoro, sync/wifi status |
| **Full** | `"full": true` | On connect + after every command | Everything in Fast + all settings |

## Client-side update rules

### Always updated (from both Fast and Full)
Time display, segment LEDs, stopwatch, timer, tabata, pomodoro states and counters, sync/wifi status dots, animation state.

### Only updated from Full state (`st.full`)
All settings controls: brightness slider, color dots, color mode radio, clock format toggle, animation toggle, colon toggle, buzzer radio, clockwork toggle, timezone, DST, night shift, pomodoro intervals, date display, birthdays, tabata presets.

This prevents fast broadcasts from overwriting user input before the server has processed the command.

## Command categories

### Instant (`send()`)
No toast. UI stays as the user set it. LEDs respond immediately.

| Control | Command | Notes |
|---------|---------|-------|
| Color dot | `color` | Instant LED change |
| Custom color | `customcolor` | Instant LED change |
| Brightness slider | `brightness` | Instant LED change, server also calls `setBrightness` directly |
| Color mode | `colormode` | Static / Rainbow / Crazy / Wave |
| Mode switch | `mode` | Tab navigation |
| LED test | `animate` | |
| Clock format | `clockfmt` | HH:MM / MM:SS toggle |
| Animation toggle | `animtoggle` | |
| Colon LEDs | `colon` | |
| Buzzer level | `buzzer` | Off / Low / High |
| Clockwork chime | `clockwork` | |
| Buzzer test | `buzztest` | |
| Tabata config | `tabata_cfg` | Work/rest/intervals/colors |
| Pomodoro intervals | `pom_cfg` | |
| Stopwatch | `sw` | start / stop / reset / restart |
| Timer | `timer` | set / start / stop / reset |
| Tabata | `tabata` | start / stop / reset |
| Pomodoro | `pom` | start / stop / reset |
| Reset WiFi | `resetwifi` | Device restarts |

### Confirmed (`sendSave()`)
Shows a "Saving..." toast. Toast disappears when the server echoes back a Full state.

| Control | Command | Notes |
|---------|---------|-------|
| Timezone | `timezone` | |
| DST mode | `dst` | Off / Custom / Always On |
| DST rules | `dst_rules` | Start/end rule details |
| DST Israel default | `dst_reset_israel` | |
| Night shift | `nightshift` | Enable, start/end hour, brightness |
| Date display | `datedisp` | Enable, interval |
| Add birthday | `bday_add` | |
| Delete birthday | `bday_del` | |
| Load tabata preset | `tab_preset_load` | |
| Save tabata preset | `tab_preset_save` | |
| Delete tabata preset | `tab_preset_del` | |

## Saving toast

- Fixed-position pill at top center: "Saving..."
- Shown by `sendSave()` before sending the command
- Hidden when a Full state message (`st.full`) arrives back
- Never shown for instant controls

## Brightness state machine (main.cpp)

Single authority for LED brightness, runs every loop iteration:

```
if (nightShift active && time synced) → nightShiftBrightness
else                                  → settings.brightness
```

The WebUI brightness handler calls `setBrightness()` directly for instant response. The state machine re-applies the correct brightness on the next loop if night shift overrides it.

## Countdown animation (Timer & Tabata)

During the last 5 seconds of a timer or tabata phase, the fade transition animation plays on **all 4 digits** simultaneously (not just the digit that changed). This creates a dramatic countdown effect.

- Controlled by the `animateTransitions` setting (same toggle as clock transitions)
- Uses `showNumberFadeAnimated(number, allDigits=true)`
- Outside the last 5 seconds, display updates normally via `showAndMirror()`

## Removed features
- **Gym mode** — fully removed (settings, NVS, WebUI, brightness override)
- **Sunrise auto-color** — fully removed (settings, NVS, WebUI, main loop)
