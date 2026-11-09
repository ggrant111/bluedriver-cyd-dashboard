#pragma once
// Pure C++11 helpers based on published third-party BlueDriver BLE research.
// NOT validated against this project's physical BlueDriver adapter.
#include <stddef.h>
#include <stdint.h>

namespace bluedriver {
// XOR transform is symmetric. Reported exceptions: CR and ELM prompt.
inline uint8_t transform(uint8_t byte) {
  return (byte == 0x0D || byte == 0x3E) ? byte
                                        : static_cast<uint8_t>(byte ^ 0x26);
}
inline void fragmentAck(uint8_t length, uint8_t out[3]) {
  out[0] = 0x01;
  out[1] = length; // Exact notification byte count, not decoded character count.
  out[2] = 0x00;
}
inline void receiveCredit(uint8_t out[3]) {
  out[0] = 0x00; out[1] = 0xFF; out[2] = 0x7F;
}

class ResponseBuffer {
 public:
  ResponseBuffer() { reset(); }
  void reset() { size_ = 0; completed_ = false; overflow_ = false; data_[0] = '\0'; }
  // Feed each incoming DATA-notification payload in order. BLE transport must
  // promptly ACK EACH fragment; no work is performed on the vehicle here.
  bool appendEncoded(const uint8_t* bytes, size_t n) {
    if (!bytes || completed_ || overflow_) return false;
    for (size_t i = 0; i < n; ++i) {
      if (size_ >= sizeof(data_) - 1) {
        overflow_ = true;
        data_[size_] = '\0';
        return false;
      }
      const char decoded = static_cast<char>(transform(bytes[i]));
      data_[size_++] = decoded;
      data_[size_] = '\0';
      if (decoded == '>') {
        // A prompt should end a complete response; extra data is not accepted.
        if (i + 1 < n) { overflow_ = true; return false; }
        completed_ = true;
      }
    }
    return completed_;
  }
  bool complete() const { return completed_ && !overflow_; }
  bool failed() const { return overflow_; }
  const char* text() const { return data_; }
  size_t size() const { return size_; }
 private:
  char data_[256];
  size_t size_;
  bool completed_;
  bool overflow_;
};
} // namespace bluedriver
