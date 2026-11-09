#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bluedriver_session.h"
#include "obd_pids.h"

using namespace bluedriver;
static const char* kCommands[] = {
  "LMI", "LMD", "LML0", "LMI", "LMD", "LMDP", "LMI", "LMX1", "LMX0"
};
static void emit(OfflineSession& session, const char* plain, size_t partAt) {
  uint8_t wire[128];
  const size_t n = strlen(plain);
  assert(n < sizeof(wire) && partAt > 0 && partAt < n);
  for (size_t i = 0; i < n; ++i)
    wire[i] = transform(static_cast<uint8_t>(plain[i]));
  assert(session.onDataFragment(wire, partAt));
  PendingWrite ack = session.nextWrite();
  assert(ack.type == WriteType::DataAck && ack.length == 3);
  assert(ack.bytes[0] == 1 && ack.bytes[1] == partAt && ack.bytes[2] == 0);
  assert(session.onWriteResult(WriteType::DataAck, true, 100));
  assert(session.onDataFragment(wire + partAt, n - partAt));
  ack = session.nextWrite();
  assert(ack.type == WriteType::DataAck && ack.bytes[1] == n - partAt);
  assert(session.onWriteResult(WriteType::DataAck, true, 110));
}
int main() {
  OfflineSession session;
  assert(session.phase() == SessionPhase::Stopped);
  assert(session.nextWrite().type == WriteType::None);
  session.start();
  PendingWrite a = session.nextWrite();
  assert(a.type == WriteType::DataCredit && a.length == 3);
  assert(a.bytes[0] == 0 && a.bytes[1] == 255 && a.bytes[2] == 127);
  assert(session.onWriteResult(WriteType::DataCredit, true, 0));

  for (size_t i = 0; i < OfflineSession::kCommandCount; ++i) {
    assert(session.completedSteps() == i);
    assert(session.phase() == SessionPhase::NeedCommand);
    a = session.nextWrite();
    assert(a.type == WriteType::Command);
    assert(strcmp(a.command, kCommands[i]) == 0);
    const size_t commandLength = strlen(kCommands[i]);
    assert(a.length == commandLength + 1 && a.bytes[commandLength] == '\r');
    for (size_t j = 0; j < commandLength; ++j) {
      assert(transform(a.bytes[j]) == static_cast<uint8_t>(kCommands[i][j]));
    }
    assert(session.onWriteResult(WriteType::Command, true, 1000 + i * 100));
    assert(session.phase() == SessionPhase::AwaitingResponse);
    assert(session.nextWrite().type == WriteType::None); // one in-flight!
    assert(session.onCommandAck(reinterpret_cast<const uint8_t*>("\x01\x01\x00"), 3));
    assert(session.commandAckSeen());

    const char* response = (i == 0 || i == 2) ? "?\r>" :
                            (i == 5) ? "6\rOK\r>" : "OK\r>";
    emit(session, response, 2);
    assert(!session.failed());
    assert(strcmp(session.lastReply(), response) == 0);
  }
  assert(session.ready());
  assert(session.completedSteps() == OfflineSession::kCommandCount);
  assert(session.nextWrite().type == WriteType::None);
  assert(!session.onCommandAck(reinterpret_cast<const uint8_t*>("\x01\x01\x00"), 3));
  session.stop();
  assert(session.phase() == SessionPhase::Stopped && session.completedSteps() == 0);
  assert(strlen(session.lastReply()) == 0);

  // A command with two fragments MUST remain pending until the last DATA ACK.
  session.start();
  assert(session.onWriteResult(WriteType::DataCredit, true, 0));
  assert(session.onWriteResult(WriteType::Command, true, 1));
  const uint8_t first[] = {transform('O'), transform('K')};
  assert(session.onDataFragment(first, sizeof(first)));
  assert(session.completedSteps() == 0);
  assert(session.nextWrite().type == WriteType::DataAck);
  const uint8_t wrong[] = {0, 0, 0};
  assert(!session.onCommandAck(wrong, sizeof(wrong)));
  // Reject out-of-order notifications while an ACK is queued.
  assert(!session.onDataFragment(first, 1));
  assert(session.failed());

  // Timeout must fail closed, never repeat the LM write or move to next step.
  session.start();
  assert(session.onWriteResult(WriteType::DataCredit, true, 10));
  assert(session.onWriteResult(WriteType::Command, true, 100));
  session.poll(4099);
  assert(!session.failed());
  session.poll(4100);
  assert(session.failed());
  assert(session.nextWrite().type == WriteType::None);
  assert(session.completedSteps() == 0);

  // Response buffer overflow triggers an error, no live data is accepted.
  session.start();
  assert(session.onWriteResult(WriteType::DataCredit, true, 0));
  assert(session.onWriteResult(WriteType::Command, true, 0));
  uint8_t huge[255];
  memset(huge, transform('A'), sizeof(huge));
  assert(session.onDataFragment(huge, sizeof(huge)));
  assert(session.onWriteResult(WriteType::DataAck, true, 1));
  assert(!session.onDataFragment(huge, sizeof(huge)));
  assert(session.failed());

  // Write errors must fail closed. No automatic resend/retry on a dirty link.
  session.start();
  assert(!session.onWriteResult(WriteType::DataCredit, false, 0));
  assert(session.failed());
  session.start();
  assert(session.onWriteResult(WriteType::DataCredit, true, 0));
  assert(!session.onWriteResult(WriteType::Command, false, 0));
  assert(session.failed());

  // The base PID decoder should still handle a BlueDriver-style prompt.
  float rpm = 0;
  assert(obd::decodeMode01(0x0C, "0F 80 \r>", rpm) && rpm == 992);
  printf("PASS: 9-step LM setup, XOR command bytes, per-fragment ACK, prompt gating,\n");
  printf("      no pipelining, timeout, overlap, overflow, cancel, and write failures\n");
}
