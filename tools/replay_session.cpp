// Offline, deterministic BlueDriver session replay. DOES NOT USE BLUETOOTH.
// Build: g++ -std=c++11 -Wall -Wextra -Werror -Iinclude tools/replay_session.cpp -o /tmp/replay_session
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "bluedriver_session.h"
#include "obd_pids.h"

static void printHex(const uint8_t* bytes, size_t len) {
  for (size_t i = 0; i < len; ++i) printf("%02X%s", bytes[i], i + 1 == len ? "" : " ");
}

int main() {
  using namespace bluedriver;
  static const char* kReplies[9] = {
    "?\r>", "OK\r>", "?\r>", "OSAPI v2.60\rOK\r>", "OK\r>",
    "6\rOK\r>", "OSAPI v2.60\rOK\r>", "OK\r>", "OK\r>"
  };
  printf("BlueDriver CYD OFFLINE REPLAY — SYNTHETIC RESPONSES ONLY\n");
  printf("No BLE device, adapter, car, or real PID data is accessed.\n\n");
  OfflineSession s;
  s.start();

  PendingWrite write = s.nextWrite();
  assert(write.type == WriteType::DataCredit);
  printf("[SIM] DATA credit: ");
  printHex(write.bytes, write.length);
  printf("\n");
  assert(s.onWriteResult(write.type, true, 0));

  for (size_t step = 0; step < OfflineSession::kCommandCount; ++step) {
    write = s.nextWrite();
    assert(write.type == WriteType::Command);
    printf("[SIM] CMD step %zu/9: %-4s wire=", step + 1, write.command);
    printHex(write.bytes, write.length);
    printf("\n");
    assert(s.onWriteResult(write.type, true, static_cast<uint32_t>(100 + 10 * step)));
    const uint8_t cmdAck[] = {0x01, 0x01, 0x00};
    assert(s.onCommandAck(cmdAck, sizeof(cmdAck)));

    const char* reply = kReplies[step];
    const size_t size = strlen(reply);
    const size_t first = size / 2;
    uint8_t encoded[64];
    assert(size < sizeof(encoded) && first > 0);
    for (size_t i = 0; i < size; ++i)
      encoded[i] = transform(static_cast<uint8_t>(reply[i]));

    const size_t chunks[2] = {first, size - first};
    size_t offset = 0;
    for (size_t c = 0; c < 2; ++c) {
      assert(s.onDataFragment(encoded + offset, chunks[c]));
      PendingWrite ack = s.nextWrite();
      assert(ack.type == WriteType::DataAck);
      printf("[SIM]   DATA fragment %zu len=%zu ACK=", c + 1, chunks[c]);
      printHex(ack.bytes, ack.length);
      printf("\n");
      assert(s.onWriteResult(WriteType::DataAck, true,
                             static_cast<uint32_t>(105 + 10 * step)));
      offset += chunks[c];
    }
    printf("[SIM]   prompt complete; advanced %zu/9\n", s.completedSteps());
  }

  assert(s.ready());
  printf("\n[SIM] READY (offline only; NOT a connected scanner)\n");
  float rpm = 0;
  assert(obd::decodeMode01(0x0C, "0F 80 \r>", rpm));
  printf("[SIM] Example synthetic PID 010C = %.0f RPM (NOT vehicle telemetry)\n", rpm);
  return 0;
}
