#include <assert.h>
#include <stdio.h>
#include "ble_identity.h"
int main() {
  assert(bleidentity::isLikelyBlueDriver("BlueDriver"));
  assert(bleidentity::isLikelyBlueDriver("BlueDriv-ABC"));
  assert(bleidentity::isLikelyBlueDriver("bluedriver"));
  assert(bleidentity::isLikelyBlueDriver("LSB2-1234"));
  assert(bleidentity::isLikelyBlueDriver("lsb2-"));
  assert(!bleidentity::isLikelyBlueDriver(""));
  assert(!bleidentity::isLikelyBlueDriver(nullptr));
  assert(!bleidentity::isLikelyBlueDriver("Other OBD"));
  assert(!bleidentity::isLikelyBlueDriver("LSB22"));
  assert(!bleidentity::isLikelyBlueDriver("BlueDive"));
  assert(bleidentity::knownCharacteristic("a9da6040-0823-4995-94ec-9ce41ca28833"));
  assert(bleidentity::knownCharacteristic("A73E9A10-628F-4494-A099-12EFAF72258F"));
  assert(bleidentity::knownCharacteristic("75a9f022-af03-4e41-b4bc-9de90a47d50b"));
  assert(bleidentity::knownCharacteristic("12e868e7-c926-4906-96c8-a7ee81d4b1b3"));
  assert(!bleidentity::knownCharacteristic("00002a26-0000-1000-8000-00805f9b34fb"));
  printf("PASS: advertisement name matching and characteristic UUID registry\n");
  return 0;
}
