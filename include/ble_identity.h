#pragma once
// Hardware-independent BLE discovery filters/UUID classification.
// Keep all candidate selection explicit; never auto-connect to nearby scanners.
#include <ctype.h>
#include <stddef.h>

namespace bleidentity {
inline bool startsWithIgnoreCase(const char* text, const char* prefix) {
  if (!text || !prefix) return false;
  while (*prefix) {
    if (!*text) return false;
    const unsigned char a = static_cast<unsigned char>(*text++);
    const unsigned char b = static_cast<unsigned char>(*prefix++);
    if (tolower(a) != tolower(b)) return false;
  }
  return true;
}
inline bool isLikelyBlueDriver(const char* name) {
  return startsWithIgnoreCase(name, "BlueDriv") ||
         startsWithIgnoreCase(name, "LSB2-");
}
inline bool equalsIgnoreCase(const char* a, const char* b) {
  if (!a || !b) return false;
  while (*a && *b) {
    if (tolower(static_cast<unsigned char>(*a)) !=
        tolower(static_cast<unsigned char>(*b))) return false;
    ++a; ++b;
  }
  return *a == '\0' && *b == '\0';
}
inline const char* knownCharacteristic(const char* uuid) {
  if (equalsIgnoreCase(uuid, "a9da6040-0823-4995-94ec-9ce41ca28833")) return "CMD";
  if (equalsIgnoreCase(uuid, "a73e9a10-628f-4494-a099-12efaf72258f")) return "DATA";
  if (equalsIgnoreCase(uuid, "75a9f022-af03-4e41-b4bc-9de90a47d50b")) return "STATUS";
  if (equalsIgnoreCase(uuid, "12e868e7-c926-4906-96c8-a7ee81d4b1b3")) return "ENCRYPTED";
  return nullptr;
}
}
