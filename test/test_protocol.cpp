#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "obd_pids.h"
#include "bluedriver_protocol.h"

static void near(float actual, float expected, float tolerance = 0.02f) {
  assert(fabsf(actual - expected) <= tolerance);
}
static void check(uint8_t pid, const char* answer, float expected, float tol = 0.02f) {
  float value = -999.0f;
  assert(obd::decodeMode01(pid, answer, value));
  near(value, expected, tol);
}

int main() {
  // Published example: 0F 80 -> 0x0F80 / 4 == 992 RPM.
  check(0x0C, "0F 80 \r>", 992);
  check(0x0C, "41 0C 0F 80\r>", 992);
  check(0x0D, "64\r>", 100);
  check(0x05, "7B\r>", 83);
  check(0x0F, "50\r>", 40);
  check(0x04, "80\r>", 128.0f * 100 / 255);
  check(0x11, "FF\r>", 100);
  check(0x2F, "00\r>", 0);
  check(0x0B, "65\r>", 101);
  check(0x42, "35 26\r>", 13.606f);
  near(obd::cToF(100), 212);
  near(obd::kmhToMph(100), 62.1371f);
  float unused;
  assert(!obd::decodeMode01(0x0C, "0F 80", unused));       // partial
  assert(!obd::decodeMode01(0x0C, "NO DATA\r>", unused));  // ECU failed
  assert(!obd::decodeMode01(0x0C, "41 0D 0F 80\r>", unused)); // mismatched PID
  assert(!obd::decodeMode01(0x0C, "0F\r>", unused));       // too few bytes
  assert(!obd::decodeMode01(0x0C, "0F 80 12\r>", unused)); // too many bytes
  assert(!obd::decodeMode01(0x0C, "0F 80\r>JUNK", unused));
  assert(!obd::decodeMode01(0x99, "FF\r>", unused));

  assert(bluedriver::transform(0x0D) == 0x0D);
  assert(bluedriver::transform(0x3E) == 0x3E);
  assert(bluedriver::transform(bluedriver::transform('L')) == 'L');
  uint8_t ack[3], credit[3];
  bluedriver::fragmentAck(4, ack);
  bluedriver::receiveCredit(credit);
  assert(ack[0] == 1 && ack[1] == 4 && ack[2] == 0);
  assert(credit[0] == 0 && credit[1] == 0xFF && credit[2] == 0x7F);

  bluedriver::ResponseBuffer response;
  const char* raw = "0F 80 \r>";
  uint8_t encoded[16];
  const size_t n = strlen(raw);
  for (size_t i=0; i<n; ++i)
    encoded[i] = bluedriver::transform(static_cast<uint8_t>(raw[i]));
  assert(!response.appendEncoded(encoded, 2)); // fragmented BLE data
  assert(response.appendEncoded(encoded + 2, n - 2));
  assert(response.complete());
  assert(strcmp(response.text(), raw) == 0);
  check(0x0C, response.text(), 992);
  response.reset();
  assert(!response.complete() && response.size() == 0);
  printf("PASS: PID decode, strict response framing, BlueDriver XOR/ACK helpers\n");
}
