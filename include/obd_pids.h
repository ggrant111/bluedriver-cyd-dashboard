#pragma once
// Pure C++11 OBD-II Mode 01 decoding. No Arduino dependencies.
// Accepts completed BlueDriver-style data-only or generic '41 PID' responses.
#include <stdint.h>
#include <stddef.h>
#include <ctype.h>
#include <string.h>

namespace obd {
inline int hexNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

inline bool hexBytesFromCompleteResponse(const char* response, uint8_t* bytes,
                                         size_t capacity, size_t& count) {
  count = 0;
  if (!response || !bytes || !capacity) return false;
  const char* prompt = strchr(response, '>');
  if (!prompt) return false; // Never decode a partial BLE response.
  for (const char* p = response; p < prompt;) {
    if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') { ++p; continue; }
    if (p + 1 >= prompt || count >= capacity) return false;
    int high = hexNibble(p[0]);
    int low = hexNibble(p[1]);
    if (high < 0 || low < 0) return false;
    p += 2;
    if (p < prompt && !isspace(static_cast<unsigned char>(*p))) return false;
    bytes[count++] = static_cast<uint8_t>((high << 4) | low);
  }
  for (const char* p = prompt + 1; *p; ++p)
    if (!isspace(static_cast<unsigned char>(*p))) return false;
  return count != 0;
}

inline unsigned expectedDataBytes(uint8_t pid) {
  switch (pid) {
    case 0x0C: case 0x42: return 2;
    case 0x0D: case 0x05: case 0x0F: case 0x04:
    case 0x11: case 0x2F: case 0x0B: return 1;
    default: return 0;
  }
}

// Call only with the full prompt-terminated answer for one requested PID.
// Returns false on unsupported PID, malformed/no-data or byte-count mismatch.
inline bool decodeMode01(uint8_t pid, const char* response, float& result) {
  const unsigned expected = expectedDataBytes(pid);
  if (!expected) return false;
  uint8_t bytes[16];
  size_t n = 0;
  if (!hexBytesFromCompleteResponse(response, bytes, sizeof(bytes), n)) return false;
  size_t offset = 0;
  if (n >= 2 && bytes[0] == 0x41) {
    if (bytes[1] != pid) return false;
    offset = 2;
  }
  if (n - offset != expected) return false;
  const unsigned a = bytes[offset];
  const unsigned b = expected == 2 ? bytes[offset + 1] : 0;
  switch (pid) {
    case 0x0C: result = (256.0f * a + b) / 4.0f; return true;
    case 0x0D: result = static_cast<float>(a); return true;
    case 0x05: case 0x0F: result = static_cast<float>(a) - 40.0f; return true;
    case 0x04: case 0x11: case 0x2F:
      result = a * 100.0f / 255.0f; return true;
    case 0x0B: result = static_cast<float>(a); return true;
    case 0x42: result = (256.0f * a + b) / 1000.0f; return true;
    default: return false;
  }
}
inline float kmhToMph(float kmh) { return kmh * 0.621371192f; }
inline float cToF(float c) { return c * 1.8f + 32.0f; }
} // namespace obd
