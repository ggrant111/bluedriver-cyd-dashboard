# Offline BlueDriver Protocol Testing — v0.4 Preparation

**Status:** Simulation only. No actual BlueDriver BLE session, scanner firmware revision, CYD display, or vehicle connection has been verified by these tests.

## Why we can develop without the car

The published reverse-engineered BLE protocol specifies a credit window, a nine-command LM setup sequence, XOR framing, a DATA-fragment acknowledgment for each notification, and an ELM-style '>' prompt completing responses. These can all be validated against deterministic synthetic traffic without connecting to an OBD-II adapter.

The actual proprietary protocol may differ on the specific adapter, so simulated success is **not proof of compatibility**. We will only enable transmission in a separately reviewed hardware milestone after GATT characteristics and BLE bonding have been observed on that scanner.

**Upstream research:** https://github.com/sjalkote/bluedriver-reverse-engineering

## Commands

From the repository root:

~~~bash
g++ -std=c++11 -Wall -Wextra -Werror -Iinclude test/test_session.cpp -o /tmp/test_bluedriver_session
/tmp/test_bluedriver_session

g++ -std=c++11 -Wall -Wextra -Werror -Iinclude tools/replay_session.cpp -o /tmp/replay_session
/tmp/replay_session
~~~

On Windows using a GCC toolchain, replace the Unix output paths with local executable filenames and run them from your command prompt. Alternatively use the GitHub Actions **native-tests** job and its downloadable **bluedriver-offline-session-replay** log artifact; no PC installation or car access is required to review the trace.

## Protocol state machine

- STOPPED: no pending writes or volatile reply data.
- NEED_CREDIT: propose raw DATA write 00 FF 7F once.
- NEED_COMMAND: propose exactly one XOR-encoded, CR-terminated command.
- AWAITING_RESPONSE: require DATA fragment ACK 01 <fragment_length> 00 for every fragment; do not move on until a complete '>' prompt **and** acknowledgment of the last fragment.
- READY: all nine published LM commands completed *in the simulator*. Never treat this phase as live vehicle connectivity without actual transport.
- FAILED: fail closed on invalid ordering, write errors, overflow or 4-second command timeout. No automatic retransmit/pipelining.

The command sequence is:

| Step | ASCII command | Expected example synthetic reply |
| --- | --- | --- |
| 1 | LMI | ? |
| 2 | LMD | OK |
| 3 | LML0 | ? |
| 4 | LMI | OSAPI v2.60 / OK |
| 5 | LMD | OK |
| 6 | LMDP | 6 / OK |
| 7 | LMI | OSAPI v2.60 / OK |
| 8 | LMX1 | OK |
| 9 | LMX0 | OK |

All replies are **fixture strings for deterministic testing**, not observations from the user's scanner. The protocol does not require an exact reply string per step in this preparatory module; only completion and bounded delivery are checked. Device-specific semantic validation may be added after evidence.

## Covered test cases

- Exact command order and CR terminators, XOR transform and credit grant.
- Per-notification ACK length and ACK priority over subsequent commands.
- DATA split across multiple notifications with prompt-only completion.
- No concurrent/pipelined writes; command ACK is not DATA completion.
- Timeout behavior and failed write behavior.
- Reject overlapping DATA notifications when their previous ACK is still pending.
- Reject overlong response buffer and reset all volatile state on stop.
- Decode a **synthetic** 010C Mode 01 response into 992 RPM, always labeled as simulated.

## What still requires the actual scanner and car

1. Confirm the CYD display driver/orientation and BLE memory stability.
2. Capture nearby scanner advertising names, connect to the real adapter and record GATT properties.
3. Test pairing/security and subscriptions. Check which writes require response.
4. Compare CMD/DATA ACK behavior and notification fragmentation with this simulator.
5. Only then implement and opt-in to the nine-step LM handshake in a real BLE transport.
6. Finally perform a parked-vehicle, read-only 010C query and verify live readings; never label synthetic values as live.

## Evidence log policy

Keep synthetic tests separate from physical observations. The actual adapter's raw MAC address, VIN, pairing keys, nearby device names, and identifiable logs should not be committed. Use redacted captures and timestamps when moving from [PUBLISHED] to [PROJECT-VERIFIED].

## Out of scope

No programmatic BLE writes, AT reset commands, DTC clearance, VIN requests, manufacturer control services, or vehicle commands. Existing v0.3 discovery firmware remains unchanged by this simulation feature.
