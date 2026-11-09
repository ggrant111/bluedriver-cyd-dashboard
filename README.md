# BlueDriver CYD Dashboard

**v0.3.1 experimental** — ESP32-2432S028R (common 2.8-inch ILI9341 CYD, 320×240) Bluetooth discovery and diagnostic dashboard for a BlueDriver BLE OBD-II scanner. Primary intended vehicle: 2024 Ford Bronco Outer Banks 2.3L.

> **NOT a live car dashboard yet.** Discovery, connection, GATT inventory and opt-in BLE security are implemented in source, but have **not** been physically verified on the project CYD/BlueDriver. There are no OBD data reads, no vehicle control commands, and the demo gauge screen remains visibly labeled **DEMO**.

## Project knowledge

- [ROADMAP.md](ROADMAP.md) — phases, CI milestones, hardware validation gates
- [docs/TECHNICAL_REFERENCE.md](docs/TECHNICAL_REFERENCE.md) — BlueDriver BLE research, service/characteristic UUIDs, pinout, OBD formulas, actual evidence ledger
- [docs/OFFLINE_TESTING.md](docs/OFFLINE_TESTING.md) — v0.4 preparation: simulated LM session, error tests, replay commands, remaining hardware steps
- [docs/DASHBOARD_SIMULATOR.md](docs/DASHBOARD_SIMULATOR.md) — offline 320×240 dashboard, layouts, browser setup, touch/serial navigation

## NEW: Offline 320×240 dashboard simulator

Download the repository ZIP, extract it, and open **`simulator/index.html`** locally in your browser (keep `app.js` and `model.js` beside it). No server or vehicle required. Change among nine gauges, two pages, Grid/Focus layouts, mph/km/h and Fahrenheit/Celsius, three driving scenarios, pause and stale data. The preview always says **DEMO** and cannot connect to Bluetooth.

The CYD firmware also now boots to a **DEMO dashboard without scanning** and provides matching navigation through touch (uncalibrated; verify your physical CYD) or USB serial. On the hardware screen, tap a gauge to cycle metrics. Use PAGE/BLE/SETUP/FOCUS along the bottom. Preferences retain gauge/layout/units on reboot.

**[Full simulator and calibration guide](docs/DASHBOARD_SIMULATOR.md)**.

## Work that can run without the scanner or car

The **offline v0.4 preparation** includes `include/bluedriver_session.h`, `test/test_session.cpp` and `tools/replay_session.cpp`: a testable transport-independent BlueDriver LM initializer. It simulates the full nine-command setup, CREDIT and fragment ACK flow, no command pipelining, completion prompts, timeouts and write errors. **It is not wired into the v0.3 ESP32 BLE scanner and cannot send Bluetooth or vehicle commands.**

Run the simulator locally (C++11 compiler required):

~~~bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude tools/replay_session.cpp -o replay_session
./replay_session
~~~

GitHub Actions also publishes a downloadable **bluedriver-offline-session-replay** text log with synthetic packet traces; you can inspect it in a browser without any device.

## Build and flash

Requirements: Python 3, [PlatformIO](https://platformio.org/), the **classic ESP32** CYD. This build uses Arduino framework + **NimBLE-Arduino 1.4.3** + TFT_eSPI.

```bash
pio run -e cyd
pio run -e cyd -t upload --upload-port COM6
pio device monitor -b 115200 --port COM6
```

Use the actual serial port (e.g. COM6 on Windows, /dev/ttyUSB0 on Linux). The firmware is at `.pio/build/cyd/firmware.bin`. For download directly from CI, open the latest successful GitHub Actions run and choose **bluedriver-cyd-v0.3.1-offline-dashboard-experimental** under Artifacts. Hardware revision and LCD orientation must be checked before use; no claim is made that all CYD clones use this pinout.

## First live BLE test (parked vehicle only)

1. Park safely. Use ignition/ACC as needed and power the scanner via the OBD-II port. Close or disconnect the BlueDriver phone app so that it isn't holding the adapter connection.
2. Connect CYD by USB and open 115200-baud serial monitor. The screen should display `BLUEDRIVER / BLE` and begin a six-second scan.
3. Wait for a list matching advertised names `BlueDriv...` or `LSB2-...` with signal strength. If none appear, press `s` to rescan. Optionally press `a` for all detected devices (don't share captures containing other devices' private identifiers).
4. Press **1 through 6** to explicitly connect to an entry. Expect `[BLE] link connected`, then `[GATT] service=` and `[GATT] char=` records with UUIDs and properties. The reference characteristic count is 4, but this varies by scanner firmware.
5. If GATT is visible, press `p` to explicitly attempt a secured BLE session / bond. This is optional and may not succeed without an encrypted read on this model; note the actual result.
6. Press `x` to disconnect. Press `d` for simulated gauges (never live) or `b` to return to the BLE device list.

### Serial command reference

| Key | Action |
| --- | --- |
| `s` | scan for BlueDriver-like names (6 seconds) |
| `a` | scan for all nearby BLE devices (manual fallback) |
| `1`–`6` | connect to selected scanned device and enumerate GATT |
| `p` | manual Bluetooth secure/bond request (no ECU commands) |
| `x` | disconnect |
| `b` | BLE scanner screen |
| `d` | separate, simulated DEMO gauges |
| `h` | full serial help |

**Diagnostics to send back for the next milestone:** sanitized `[BOOT]`, `[BLE]`, `[GATT]` log lines, what the screen displayed, whether the scanner was seen, whether connection succeeded and whether pairing succeeded. Never post VIN, full MAC address, or pairing keys publicly.

## Automated testing

The GitHub Actions workflow executes both host-native C++ test suites and compiles firmware. Manual equivalents:

```bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/test_protocol.cpp -o test_protocol
./test_protocol
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/test_discovery.cpp -o test_discovery
./test_discovery
pio run -e cyd
```

A green workflow verifies the host decoders/filter tests and that a binary can be compiled; it **does not** verify a BlueDriver connection, touch calibration, a correct TFT driver, or Bronco telemetry.

## Current architecture

- `src/main.cpp`: CYD demo dashboard, settings, XPT2046 touch/serial navigation and separate BLE diagnostics.
- `include/dashboard_model.h` and `include/dashboard_navigation.h`: host-testable synthetic values, units, layout, freshness and touch hit regions.
- `simulator/`: standalone offline browser preview with locally persisted settings.
- `src/bluedriver_ble.cpp`: ESP32/NimBLE read-only discovery, explicit connection, GATT inventory and optional secured link.
- `include/ble_identity.h`: pure C++ advertisement matching + known characteristic classifier.
- `include/bluedriver_protocol.h` and `include/obd_pids.h`: pure C++ utilities for future v0.4+ protocol and PID handling, **not** used to send commands in this release.
- `test/` + `.github/workflows/ci.yml`: native tests and CI cross-build.

## Next

**Hardware bring-up, touch calibration, and pairing confirmation**, then proprietary BlueDriver LM initialization, ACK/credit handling, and first real read-only Mode 01 PID. Read-only, parked-vehicle debugging only; do not configure, use, or debug the device while driving.

Independent research project; no affiliation with BlueDriver or Ford.
