# BlueDriver CYD Dashboard

An independent **2024 Ford Bronco** live-gauge dashboard prototype targeting an **ESP32-2432S028R (2.8-inch CYD, 320×240 landscape)** and a **BlueDriver OBD-II BLE adapter**.

> **Status: v0.1 experimental.** The initial firmware draws **DEMO ONLY** gauges. It does **not** yet connect to BlueDriver, and no physical BlueDriver or vehicle validation has been performed. Never interpret demo values as car measurements.

## Project knowledge base

- [ROADMAP.md](ROADMAP.md) — milestones, acceptance gates and next steps
- [docs/TECHNICAL_REFERENCE.md](docs/TECHNICAL_REFERENCE.md) — CYD pins, published BlueDriver BLE research, commands, OBD formulas and evidence log

## Build and test

Install [PlatformIO](https://platformio.org/), then run:

~~~bash
pio run -e cyd
~~~

The binary is generated at `.pio/build/cyd/firmware.bin`. To flash a USB-connected CYD (check the device and port first):

~~~bash
pio run -e cyd -t upload --upload-port /dev/ttyUSB0
pio device monitor -b 115200
~~~

On Windows, use your current COM port instead of `/dev/ttyUSB0`, e.g. `COM6`.

Run hardware-independent native protocol and PID regression tests:

~~~bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/test_protocol.cpp -o test_bluedriver
./test_bluedriver
~~~

GitHub Actions runs both suites and uploads an **experimental** firmware.bin as a build artifact. A successful build does not certify hardware or vehicle compatibility.

## Supported in v0.1

- Landscape 320x240 dashboard for the common ILI9341 CYD.
- Flicker-reduced updates (individual value areas rather than full-screen refresh).
- Visibly marked demo RPM, speed, coolant and ECU voltage values.
- Dependency-free C++ Mode 01 PID decoder, with strict completed-response checking.
- Dependency-free helpers for third-party-reported BlueDriver XOR, ACK and BLE reassembly.
- CI: native tests + firmware compilation.

## Not yet implemented

- BLE scanning, pairing, GATT connection and the proprietary initialization sequence.
- Reading live vehicle PIDs.
- Touch controls and per-board calibration.
- OTA, preferences, Ford-specific enhanced PIDs.

## Safety

Read-only OBD telemetry is the sole goal of the initial vehicle releases. Do not use the dashboard for vehicle control or safety-critical decisions. Never configure or debug a screen while driving.

This is an independent interoperability project and is not affiliated with or endorsed by BlueDriver, Ford or their manufacturers.
