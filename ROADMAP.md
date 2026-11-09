# BlueDriver CYD Dashboard — Project Roadmap

**Repository:** ggrant111/bluedriver-cyd-dashboard  
**Primary vehicle:** 2024 Ford Bronco Outer Banks 2.3L  
**Target hardware:** ESP32-2432S028R (2.8-inch resistive-touch CYD, 320x240 landscape)  
**Data source:** BlueDriver OBD-II Bluetooth LE adapter  
**As of:** 2026-11-09  
**Status:** v0.3 BLE discovery/diagnostic firmware and native tests **PASS in CI**; v0.2 CYD hardware bring-up and physical BLE discovery/connection/pairing remain unverified.

## Goal

An independent, offline-capable in-vehicle dashboard. The CYD connects directly to the BlueDriver scanner and displays responsive, legible gauges. A phone and BlueDriver app are not required for normal operation after interoperability is established.

## Core rules

1. **Prove transport before claiming telemetry.** Treat third-party reverse-engineering notes as a hypothesis until checked against our specific adapter.
2. **Read-only vehicle access by default.** Never issue ECU write commands, DTC clear commands, firmware updates, or vehicle control requests. Standard Mode 01 live read requests only in initial releases.
3. **No fake vehicle data labeled live.** Demo/simulated data must be visually marked and isolated from BLE traffic.
4. **One change at a time when debugging.** Capture serial/BLE logs and hardware revision before changing the protocol or display wiring.
5. **Build on PRs.** Run native unit tests and compile firmware with GitHub Actions; merge to main only when green. Hardware validation is tracked separately from CI.
6. **Preserve evidence.** Each release records what was tested on the physical board, adapter, and vehicle.

## Milestones

| Version | Focus | Deliverables | Acceptance gate |
| --- | --- | --- | --- |
| v0.1.0 | Project bootstrap | PlatformIO project, basic landscape dashboard with clearly labeled DEMO values, PID decoder, docs, native test harness, CI firmware build/artifact | Host tests pass and CYD firmware compiles in CI; hardware unverified |
| v0.2.0 | CYD bring-up | Board revision check, TFT colors/orientation, touch calibration, screen refresh without full-screen flicker, stable power/boot | Device shows test screen; touch corners work; serial log saved |
| v0.3.0 | BlueDriver BLE discovery | Scan advertised names 'BlueDriv'/'LSB2-', show RSSI/device identity, explicit serial selection, GATT inventory and optional manual bond | **A/B:** tests and firmware build; **C/D:** actual CYD scanner/connection/bonding evidence required before closing milestone |
| v0.4.0 | BlueDriver proprietary session | Connect/bond as needed, subscribe CMD/DATA/STATUS, implement XOR, credit window/fragment ACKs, exact LM setup sequence, sequential command runner | Scanner reaches ready state; one known PID returns a complete answer with '>' delimiter |
| v0.5.0 | Vehicle data | Capability detection; serial polling of standard Mode 01 RPM, speed, coolant, load, throttle, intake air, fuel level, ECU voltage | At least RPM, speed, coolant, voltage confirmed against known state/independent reader |
| v0.6.0 | Dashboard UX & reliability | Gauge layout, freshness/timeout indicators, stable redraw and reconnection, settings persistence, user-selectable units | Sustained driving-session bench/passenger test without misleading stale readings |
| v0.7.0 | Ford-specific investigation | Document and test manufacturer-enhanced PIDs (e.g. trans temp, boost) as opt-in read-only profiles | Each added PID has verified ECU address, units, formula, range, and vehicle evidence |
| v0.8.0 | Diagnostics and refinements | Read-only DTC reporting (opt-in), serial diagnostics, BLE troubleshooting, configurable gauge pages | Logs informative; no write/clear operations |
| v0.9.0 | Distribution | OTA/update architecture, firmware release assets and flashing instructions | Recovery flash tested; artifact hash and board identity published |
| v1.0.0 | Stable release | Test matrix, documentation, release manifest, supported model limitations | All stable criteria met on target hardware and vehicle |

## Implementation: v0.3 BLE discovery (hardware validation pending)

- [x] Add BLE name matching/UUID registry and native unit tests.
- [x] Add bounded 6-second active scan, 6 candidates max, RSSI and on-screen names.
- [x] Explicit connection and GATT characteristic/property enumeration over NimBLE.
- [x] Optional manual BLE security request (serial 'p'); no automatic bonding or OBD writes.
- [x] Preserve separate, prominently labeled DEMO mode.
- [x] CI firmware compile and native tests for v0.3 — [passing run #22](https://github.com/ggrant111/bluedriver-cyd-dashboard/actions/runs/42714380035) (2026-11-09).
- [ ] Flash actual CYD; verify LCD, serial UX, scan and GATT; collect redacted evidence.
- [ ] Confirm physical pairing and stable reconnection; touch calibration remains v0.2 work.

## Historical work — v0.1

- [x] Establish this repository and feature branch.
- [x] Add a maintained roadmap and technical reference.
- [x] Implement starter firmware, offline gauge simulator and PID decoder.
- [x] Run native parser tests and compile a CYD binary in GitHub Actions ([passing CI run #4](https://github.com/ggrant111/bluedriver-cyd-dashboard/actions/runs/42697990863), 2026-11-09).
- [ ] Confirm actual CYD LCD/controller revision and flash on hardware.
- [ ] Capture real BlueDriver advertisements and GATT characteristics.

## Verification ladder

**Level A — native tests:** Pure C++ parsers/formulas pass with g++ (can run on any CI host).  
**Level B — cross-compile:** PlatformIO builds firmware.bin for esp32dev/CYD.  
**Level C — CYD device:** Flash, display orientation, frame rate, touch and serial diagnostics verified.  
**Level D — BlueDriver adapter:** Connect/pair, protocol framing/ACKs and initialization verified.  
**Level E — vehicle:** Live PIDs checked in the Bronco under controlled stationary conditions.  
**Level F — road usability:** Passenger-only interaction and readability tests; never manipulate the device while driving.

Passing A/B does NOT imply C/D/E/F.

## Proposed software boundaries

- 'src/main.cpp': physical display/UI and lifecycle.
- 'include/obd_pids.h': platform-independent, pure PID decode functions.
- 'include/bluedriver_protocol.h' (future): XOR, reassembly and state machine, host-testable.
- 'src/bluedriver_ble.cpp': ESP32/NimBLE discovery, explicit connect, GATT diagnostic and manual pairing (v0.3).
- 'test/': host-native deterministic unit tests and captured/replayed protocol fixtures.
- '.github/workflows/ci.yml': test, firmware build and uploaded binary artifact.

## Open risks / decisions

- Specific BlueDriver firmware may differ from the public reverse-engineered implementation.
- Some adapters require BLE SMP bonding; confirm ESP32 supports the necessary security flow.
- CYD clones differ in LCD driver, color order and touch wiring.
- Model-specific Ford PIDs are unknown until verified: do not manufacture definitions.
- Live refresh must be adaptive. Do not pipeline requests; maintain one in-flight query.
- BLE scanner may be claimed by a paired phone; disconnect the phone's BlueDriver app during tests.
- Vehicle charging and USB noise can destabilize power; evaluate real installation later.

## Definition of done for each feature

Source change + test + CI evidence + dated hardware evidence (if applicable) + updated docs. Explicitly mark features not verified on a vehicle.

## Source of truth

See [docs/TECHNICAL_REFERENCE.md](docs/TECHNICAL_REFERENCE.md) for confirmed board pins, third-party BLE protocol notes, Mode 01 formulas and source links. Update the reference whenever a physical test contradicts an assumption.
