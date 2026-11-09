# Technical Reference — BlueDriver → ESP32 CYD → 2024 Ford Bronco

**Revision:** 2026-11-09 (v0.3 discovery implementation) • **Scope:** Interoperability research, not a claim of working vehicle connectivity.  
**Evidence grades:** [PUBLISHED] third-party published information, [PROJECT-VERIFIED] demonstrated on our hardware (none yet), [PROPOSED] design choice, [UNKNOWN] needs measurements.

## Hardware targets

[PUBLISHED] Reference board: ESP32-2432S028R (classic ESP32-WROOM-32), typically 4 MB flash and 2.8-inch 240x320 SPI display, used in 320x240 landscape. The common ILI9341 + XPT2046 configuration below is *not universal*: some newer CYDs use ST7789 and other variants. Confirm the exact board revision before replacing the driver.

| Component | Signal | GPIO |
| --- | --- | --- |
| TFT (HSPI) | MOSI / MISO / CLK | 13 / 12 / 14 |
| TFT | CS / DC / RST | 15 / 2 / -1 (often tied to reset) |
| Backlight | active high | 21 |
| XPT2046 | MOSI / MISO / CLK | 32 / 39 / 25 |
| XPT2046 | CS / IRQ | 33 / 36 |
| SD card | CS / MOSI / MISO / CLK | 5 / 23 / 19 / 18 |
| RGB LED | R / G / B | 4 / 16 / 17 (active low on common boards; verify physical unit) |
| Speaker amp | PWM/DAC candidate | 26 (board-specific; not used in v0.1) |

[IMPLEMENTED IN FIRMWARE, HARDWARE UNVERIFIED] Toolchain: PlatformIO + Arduino core for ESP32, TFT_eSPI for LCD, NimBLE-Arduino **1.4.3** for BLE. XPT2046 touch support/calibration still planned; v0.3 uses the USB serial console for intentional pairing and selection. Keep ESP32 display and touch SPI buses separate. Allocate minimal heap; no PSRAM is expected on common CYD boards.

**References:**  
- https://github.com/JamesDavid/Overhead/blob/main/CYD-ESP32-2432S028R.md  
- https://github.com/edemko/esp32-2432s028r-idf-clock  
- https://github.com/chanslor/ESP32-2432S028R/blob/master/User_Setup.h

## v0.3 BLE discovery / reconnaissance implementation

**Status:** v0.3 host-native tests and CYD cross-compilation **passed** ([CI run #22](https://github.com/ggrant111/bluedriver-cyd-dashboard/actions/runs/42714380035)); **not validated on actual CYD/BlueDriver hardware**. Scanner does not exchange OBD-II data.

- On boot, NimBLE starts a **6-second active BLE scan** and looks for advertised names beginning with `BlueDriv` or `LSB2-` (case-insensitive). Maximum 6 candidates in the UI. Ignores unknown BLE advertisements by default.
- Serial `s` rescans BlueDriver names; `a` scans and lists ALL devices as an opt-in discovery fallback. Never connect to unknown devices without verifying that they are yours; all-device logs may reveal private names, so keep them private.
- Serial `1` through `6` explicitly selects a candidate and connects, then reads its GATT service and characteristic **metadata** (UUID, properties). No writes or vehicle requests.
- Serial `p` explicitly attempts a BLE **Just Works** secured session/bond on the selected linked device. Bluetooth security must be tested against the physical BlueDriver version; reported pairing may require an encrypted characteristic read in future revisions.
- Serial `x` disconnects; `b` shows BLE screen; `d` shows simulated gauge screen; `h` prints help. Serial baud 115200.
- BLE scan and GATT enumeration may temporarily block the main UI task; the screen displays a scan status before starting. Optimization after first successful hardware trace.
- Scanned addresses are kept in RAM only; serial log masks most of the MAC address. **Never commit raw MAC, VIN, BLE pairing secrets, or personally identifying scan captures.**
- On disconnect, the BLE state changes to `Link lost`. The demo gauge screen remains explicitly labeled `DEMO` even when BLE is connected.
- The characteristic registry recognizes CMD, DATA, STATUS and ENCRYPTED UUIDs. A `known: 4/4` report would only validate GATT UUID presence, not the proprietary command sequence or live car data.

**Verification commands and expected log prefixes:** `pio run -e cyd`, `pio device monitor -b 115200`. Expected `[BOOT]`, `[BLE]` and `[GATT]`. On-target screenshots/serial captures are required for project verification.

Reference for NimBLE version split: https://github.com/h2zero/NimBLE-Arduino/blob/master/docs/1.x_to2.x_migration_guide.md (v0.3 targets **1.4.3**, which uses second-based blocking scan and pointer-based service/characteristic vectors).

## Offline LM protocol simulator (v0.4 pre-hardware)

[SIMULATED ONLY] The new pure C++ state engine in `include/bluedriver_session.h` models the nine published LM commands, CR/XOR framing, raw 00 FF 7F credit, per-DATA-fragment 01 LEN 00 acknowledgments, sequential response completion at '>', and fail-closed timeout/error paths. The replay CLI and tests use synthetic bytes and do **not** open a Bluetooth connection, write to CMD/DATA, or retrieve any live vehicle information. This engine is **not connected to the ESP32 firmware** yet.

Detailed commands, negative tests, and criteria for the first real scanner session: [OFFLINE_TESTING.md](OFFLINE_TESTING.md). Passing native simulations and firmware compilation means level A/B only, never C/D/E.

## BlueDriver BLE interoperability research

**Primary third-party reference:** https://github.com/sjalkote/bluedriver-reverse-engineering

[PUBLISHED] Reverse engineering was captured from an iOS/BlueDriver session and implemented by that author in Python/bleak. This is independent work, not BlueDriver official documentation. It may vary with scanner model/firmware; **our own adapter is not yet tested**.

### Discovery and identification

- Expected BLE advertising name prefixes: 'BlueDriv' or 'LSB2-'.
- Reported BLE chipset/firmware layer: Silicon Labs BGX13P with an ELM327-style OBD bridge.
- Standard Device Information GATT: firmware revision '0x2A26'; model '0x2A24'.
- Another device (phone / app) connecting at same time may prevent pairing.

### Reported GATT characteristics

| Name | UUID | Purpose |
| --- | --- | --- |
| CMD | a9da6040-0823-4995-94ec-9ce41ca28833 | XOR command writes, ACK notifications |
| DATA | a73e9a10-628f-4494-a099-12efaf72258f | receive-credit writes, response notifications, per-fragment ACK |
| STATUS | 75a9f022-af03-4e41-b4bc-9de90a47d50b | status read/notifications |
| ENCRYPTED | 12e868e7-c926-4906-96c8-a7ee81d4b1b3 | protected read reportedly triggers SMP bond/pairing |

**Important:** These are characteristic UUIDs, not necessarily advertised service UUIDs. Enumerate services dynamically rather than assuming a service identifier. Confirm characteristic properties before accessing them.

### Reported wire protocol

- Commands: ASCII terminated in CR, then byte-wise XOR with key **0x26**, EXCEPT bytes 0x0D (CR) and 0x3E ('>') are sent unchanged.
- CMD notification ACK: '01 01 00' (reported).
- DATA initial credit grant: raw bytes '00 FF 7F' written before commands.
- For each DATA notification fragment, immediately send raw '01 LEN 00' to DATA, where LEN is the exact number of bytes in that fragment (subject to actual protocol verification; don't blindly assume a 255+ payload fits a byte).
- DATA bytes are XOR-decoded and accumulated until ELM-style '>' prompt; **do not parse early fragment data as a finished reply**.
- Single-PID Mode 01 responses may omit '41 PID' and contain only hex response bytes; do not force generic ELM formatting onto them.
- No pipelining: send the next query ONLY after the previous response has reached '>' or timed out and the transport state is recovered.
- Never perform an 'ATZ' or other AT reset after the LM initialization; the published report says AT setup can put its proprietary overlay into a 'NO DATA' state.

### Reported initialization order (must test)

1. Subscribe to notifications (CMD, DATA; optionally STATUS) and obtain required BLE security.
2. DATA write: '00 FF 7F' (raw, not XOR-encoded).
3. CMD 'LMI' -> wait until '>'.
4. CMD 'LMD' -> wait until '>'.
5. CMD 'LML0' -> wait until '>'.
6. CMD 'LMI' -> wait until '>'.
7. CMD 'LMD' -> wait until '>'.
8. CMD 'LMDP' -> wait until '>'.
9. CMD 'LMI' -> wait until '>'.
10. CMD 'LMX1' -> wait until '>' (exhaustive timing).
11. CMD 'LMX0' -> wait until '>' (fast timing for live gauges).
12. Query one supported read-only PID e.g. '010C', await complete prompt, validate.

**Note:** Some commands receive '?' or various version text; validate prompt framing rather than a single hard-coded success string. Retain raw hex and decoded ASCII in a redacted serial debug trace.

### State machine proposal

DISCONNECTED → SCANNING → CONNECTING → GATT_DISCOVERY → (PAIRING as required) → SUBSCRIBING → CREDIT_OPEN → INIT_STEP_1..9 → READY → REQUEST_IN_FLIGHT → READY. On disconnect/error/timeouts: reject stale buffer, show no connection, and reconnect with bounded backoff. Never display previous readings as live after timeout.

## Standard OBD-II data (Mode 01)

[PUBLISHED] Formula references: https://obd-data.com/pids/ and https://github.com/sjalkote/bluedriver-reverse-engineering

| Request | Parameter | Bytes | Formula | Display |
| --- | --- | --- | --- | --- |
| 010C | Engine speed | A,B | ((256*A)+B)/4 | rpm |
| 010D | Vehicle speed | A | A | km/h, ×0.621371 for mph |
| 0105 | Engine coolant temp | A | A-40 | °C, °F=°C×9/5+32 |
| 010F | Intake air temp | A | A-40 | °C |
| 0104 | Calculated engine load | A | A×100/255 | % |
| 0111 | Absolute throttle | A | A×100/255 | % |
| 012F | Fuel level input | A | A×100/255 | % |
| 010B | Intake manifold abs pressure | A | A | kPa (NOT boost) |
| 0142 | Control module voltage | A,B | (256*A+B)/1000 | V |

**Important:** PID availability varies by ECU; determine supported PID bitmaps ('0100', '0120', ...) and do not assume all nine requests work on this vehicle. '0142' is ECU-reported voltage, not necessarily direct battery terminal voltage. Manifold absolute pressure is not gauge boost; boost requires comparing to barometric pressure, and accuracy depends on ECU reporting.

### Parsing caveats

- BlueDriver report: single PID response may be '0F 80 \r>' rather than '41 0C 0F 80'. For 0F80, RPM=992.
- Generic ELM327 adapters usually include mode/PID bytes and can echo commands. Parser shall accept only formats explicitly selected/validated for the current transport.
- Reject 'NO DATA', '?', partial responses, stale notifications, malformed hex, and out-of-range payload counts. Keep freshness timestamp per gauge.
- PIDs are read-only; never send OBD Mode 04 (clear diagnostic information) or manufacturer-specific write/control commands.
- A built-in demo profile must always be visibly labeled 'DEMO', and should never be mistaken for real vehicle telemetry.

## Bronco-specific enhanced readings: not yet verified

[UNKNOWN] Transmission oil temperature, turbocharger target/actual boost, 4WD/transfer-case status, tire pressures, battery SOC, and other manufacturer-specific PIDs. Their CAN IDs, service numbers, request/response addressing, formulas, endian rules, and supported model-year applicability must be captured and independently confirmed before use. **Do not paste arbitrary Ford PIDs from the internet and send them to the vehicle.**

## Safety and bench-testing rules

1. Begin with ignition/ACC on and vehicle securely parked; never pair/debug while driving.
2. Inspect serial output at 115200; capture complete error messages and firmware SHA.
3. Test BLE with phone BlueDriver app closed/disconnected; power-cycle scanner only as needed.
4. Avoid logging VIN, precise location, pairing secrets, or personal identifiers in committed fixtures.
5. Sensor readings are informational; no safety-critical decisions, engine control, or vehicle modifications.
6. Until real hardware proves it, classify firmware as **unverified/experimental** even when CI builds.

## Validation record (fill in as testing proceeds)

| Item | Target | Result | Date / evidence |
| --- | --- | --- | --- |
| Native decoder tests | host g++ | **PASS** | 2026-11-09, [CI run #4](https://github.com/ggrant111/bluedriver-cyd-dashboard/actions/runs/42697990863) |
| PlatformIO CYD compilation | ESP32-2432S028R | **PASS (cross-compile only)** | 2026-11-09, [CI run #4](https://github.com/ggrant111/bluedriver-cyd-dashboard/actions/runs/42697990863) |
| LCD driver and rotation | user's CYD | Pending | — |
| Touch calibration | user's CYD | Pending | — |
| BLE advertisement & UUIDs | user's BlueDriver | **CI compile PASS; physical verification pending** | [v0.3 CI run #22](https://github.com/ggrant111/bluedriver-cyd-dashboard/actions/runs/42714380035) |
| BLE SMP/credit/LM init | user's BlueDriver | **Physical testing pending; separate offline LM simulator implemented** | [OFFLINE_TESTING.md](OFFLINE_TESTING.md) |
| RPM / speed / coolant / voltage | 2024 Bronco 2.3L | Pending | — |

**Keep this document synchronized with ROADMAP.md and serial capture evidence.** Never upgrade PUBLISHED to PROJECT-VERIFIED without test details.
