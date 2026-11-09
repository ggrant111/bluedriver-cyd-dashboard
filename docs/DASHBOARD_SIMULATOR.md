# Dashboard Simulator — v0.3.1 (Offline)

## Purpose

This milestone is designed for development **without the vehicle, BlueDriver dongle, or ESP32 hardware**. It supplies a responsive browser preview of the **exact 320 × 240 logical CYD canvas**, plus a matching hardware demo UI in firmware. All gauge readings are deterministic **synthetic** values; no values are sourced from BLE/OBD.

## Browser simulator: run locally

1. Open the repository and choose **Code → Download ZIP**, then extract the entire archive.
2. Open `simulator/index.html` in a browser (Chrome, Safari, Edge or Firefox). Keep `model.js` and `app.js` alongside it.
3. No web server, internet access, API key, Bluetooth, or car is needed.
4. The preview can be viewed on desktop or a phone; the virtual display always uses a **320 × 240 coordinate system** and scales visually to fit the browser.
5. Click a gauge to cycle metrics. Choose the slot and metric from the control panel for a specific placement.
6. Try **Grid** and **Focus**, **imperial** and **metric** units, **Idle**, **Cruise**, and **Accelerate** scenarios, and **Pause** to demonstrate delayed/stale readings.
7. Settings persist locally when browser storage is permitted. Synthetic sensor readings and their timestamps are never persisted; they cannot become fake "live" readings.

**No hosted Pages URL is required.** GitHub's normal source-file preview is not the simulator; download/extract it or use a local static server. This release does not configure GitHub Pages.

## Layout contract (320 × 240)

- Top header: y=0–34, with permanent orange **DEMO** label.
- Interactive gauges: y=40–204.
  - 2 × 2 Grid: four 148 × 80 cells, with x=9/163 and y=40/124.
  - Focus: primary 302 × 88 at x=9/y=40, three secondary 96 × 70 cells at y=134.
- Bottom navigation: y=211–239, four 80-pixel-wide touch targets.
- Dashboard footer: PAGE / BLE / SETUP / FOCUS or GRID.
- Setup screen controls: UNITS / LAYOUT / SCENARIO / SIM DATA; bottom DASH / BLE / PAGE / RESET.
- BLE screen remains diagnostics-only and does **not** drive demo telemetry. The browser BLE screen deliberately does not access any Bluetooth API.

### Metrics and default pages

| Slot | Page 1 (Drive) | Page 2 (Engine) |
|---|---|---|
| 1 | Engine RPM | Engine load |
| 2 | Speed | Throttle |
| 3 | Coolant temperature | Intake air temperature |
| 4 | ECU voltage | Fuel level |

Additional selectable metric: manifold **absolute** pressure (MAP). MAP is *not* an inferred turbo boost reading.

- Imperial (default): mph, °F.
- Metric: km/h, °C.
- Voltage in V, RPM in rpm, percentages as %, absolute manifold pressure in kPa.
- Synthetic sample cadence: 650 ms. After **1500 ms** without an update, the per-gauge badge changes to LAG; after **3500 ms**, it changes to STALE. There is no real-vehicle freshness source yet.
- Tapping a gauge cycles among the nine supported metric identities.
- Browser gauge selection and settings are saved in localStorage. Firmware uses ESP32 Preferences (NVS) for layout/gauge selection, with no shared/cloud synchronization.

## CYD firmware demo

The actual CYD firmware boots **directly to the demo dashboard** even when no scanner is near. This avoids unnecessary BLE scans and makes hardware bring-up possible from a USB port alone. It retains the v0.3 BLE diagnostic scanner on the BLE screen, with manual scanning/connecting only.

### Build / flash

```bash
pio run -e cyd
pio run -e cyd -t upload --upload-port COM6
pio device monitor -b 115200 --port COM6
```

Use your current port in place of COM6. A successful CI cross-build is not proof that your board's LCD and touch are calibrated.

### Serial navigation without touch

| Key | Action |
|---|---|
| d / b / g | Dashboard / BLE / Settings screen |
| n | Next metric page |
| u / l | Toggle units / grid vs focus |
| c / f | Next driving scenario / pause or resume demo data |
| 1 through 4 | Cycle gauge slot while on Dashboard |
| r | Restore default layout and metric selection |
| s / a | Manual BlueDriver-name scan / all-device scan while on BLE screen |
| 1 through 6 | Connect selected BLE candidate while on BLE screen |
| p / x | Explicit optional BLE bond / disconnect |
| t | Toggle raw touch-coordinate diagnostics in serial |
| h | Print help |

### Touch wiring and calibration

The common **ESP32-2432S028R** CYD uses XPT2046 on a separate SPI bus: SCLK 25, MISO 39, MOSI 32, CS 33, IRQ 36. Firmware includes XPT2046_Touchscreen 1.4 and currently applies *approximate raw coordinate mapping* to 320×240.

**Not validated on the user's actual CYD.** Some clones swap, invert, or rotate touch axes or use a different controller. If touch positions do not correspond to the screen:

1. Open 115200-baud serial monitor and enter `t` to enable debug.
2. Tap the four visual corners, note `[TOUCH] raw=(x,y,z) mapped=(x,y)` logs.
3. Do not blindly remap touch coordinates until all four corners are observed; use serial controls meanwhile.
4. Preserve a log/screenshot with board controller revision and firmware SHA for the next calibration patch.

UI repaint strategy: only the values inside each gauge card are updated at 650 ms cadence; screen chrome is repainted on navigation. This reduces full-screen flashing, but hardware flicker performance remains untested.

## Tests and CI

- `test/test_dashboard.cpp`: host-native C++ tests for model defaults, page/metric cycling, unit conversions, pause/freshness thresholds, millis rollover and 320×240 tap routing.
- `test/test_browser_model.js`: Node tests for browser-simulator model defaults, scenarios, units, settings persistence, synthetic-only behavior and UI source contracts.
- Existing protocol, BLE identity and offline LM session tests remain active.
- GitHub Actions builds the ESP32 firmware and publishes **bluedriver-cyd-v0.3.1-offline-dashboard-experimental** plus the offline LM replay artifact.

### Physical acceptance still pending

This is **not v0.4 vehicle interoperability** and not v0.6 road-tested dashboard UX. Verify LCD controller, touch mapping, touch rejection/debouncing, OLED-style redraw stability, and responsiveness on the actual CYD. BLE protocol, ECU PID support and 2024 Bronco telemetry remain explicitly unverified.
