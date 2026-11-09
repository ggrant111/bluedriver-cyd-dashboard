#pragma once
// BlueDriver v0.4 PREPARATION: deterministic, transport-free session state machine.
// This module does NOT write to BLE. The v0.3 firmware does not invoke it.
// Published wire protocol is unverified on our physical BlueDriver adapter.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "bluedriver_protocol.h"

namespace bluedriver {
enum class SessionPhase : uint8_t {
  Stopped, NeedCredit, NeedCommand, AwaitingResponse, Ready, Failed
};
enum class WriteType : uint8_t { None, DataCredit, Command, DataAck };

struct PendingWrite {
  WriteType type;
  uint8_t bytes[24];
  size_t length;
  const char* command; // diagnostics only; non-null for Command
  PendingWrite() : type(WriteType::None), length(0), command(nullptr) {
    memset(bytes, 0, sizeof(bytes));
  }
};

class OfflineSession {
 public:
  static const uint32_t kResponseTimeoutMs = 4000;
  static const size_t kCommandCount = 9;
  OfflineSession() { stop(); }

  void stop() {
    phase_ = SessionPhase::Stopped;
    completed_ = 0;
    ackPending_ = false;
    ackLength_ = 0;
    promptSeen_ = false;
    commandAckSeen_ = false;
    commandStarted_ = 0;
    lastError_ = "none";
    lastReply_[0] = '\0';
    response_.reset();
  }
  void start() {
    stop();
    phase_ = SessionPhase::NeedCredit;
  }

  SessionPhase phase() const { return phase_; }
  size_t completedSteps() const { return completed_; }
  bool ready() const { return phase_ == SessionPhase::Ready; }
  bool failed() const { return phase_ == SessionPhase::Failed; }
  bool commandAckSeen() const { return commandAckSeen_; }
  const char* lastError() const { return lastError_; }
  const char* lastReply() const { return lastReply_; }
  const char* currentCommand() const {
    return completed_ < kCommandCount ? commands()[completed_] : nullptr;
  }

  // Must be drained and confirmed before processing another received fragment.
  // The DATA fragment ACK always takes precedence over the next LM command.
  PendingWrite nextWrite() const {
    PendingWrite out;
    if (phase_ == SessionPhase::Stopped || phase_ == SessionPhase::Failed ||
        phase_ == SessionPhase::Ready) return out;
    if (ackPending_) {
      out.type = WriteType::DataAck;
      out.length = 3;
      fragmentAck(ackLength_, out.bytes);
    } else if (phase_ == SessionPhase::NeedCredit) {
      out.type = WriteType::DataCredit;
      out.length = 3;
      receiveCredit(out.bytes);
    } else if (phase_ == SessionPhase::NeedCommand) {
      out.type = WriteType::Command;
      out.command = currentCommand();
      if (!out.command) return PendingWrite();
      size_t n = strlen(out.command);
      if (n + 1 > sizeof(out.bytes)) return PendingWrite();
      for (size_t i = 0; i < n; ++i)
        out.bytes[i] = transform(static_cast<uint8_t>(out.command[i]));
      out.bytes[n] = 0x0D; // CR is sent unchanged
      out.length = n + 1;
    }
    return out;
  }

  // Only a future, isolated transport adapter may call this after a successful
  // GATT write. The session intentionally has no BLE device access of its own.
  bool onWriteResult(WriteType type, bool success, uint32_t nowMs) {
    if (failed() || phase_ == SessionPhase::Stopped ||
        phase_ == SessionPhase::Ready) return false;
    if (!success) { fail("BLE write failed"); return false; }

    if (type == WriteType::DataAck && ackPending_) {
      ackPending_ = false;
      if (promptSeen_) completeResponse();
      return !failed();
    }
    if (type == WriteType::DataCredit &&
        phase_ == SessionPhase::NeedCredit && !ackPending_) {
      phase_ = SessionPhase::NeedCommand;
      return true;
    }
    if (type == WriteType::Command &&
        phase_ == SessionPhase::NeedCommand && !ackPending_) {
      phase_ = SessionPhase::AwaitingResponse;
      commandStarted_ = nowMs;
      commandAckSeen_ = false;
      promptSeen_ = false;
      response_.reset();
      return true;
    }
    fail("Unexpected write confirmation");
    return false;
  }

  // CMD characteristic's reported 01 01 00 command ACK.
  // A CMD ACK does not complete the request: only DATA's '>' does.
  bool onCommandAck(const uint8_t* bytes, size_t size) {
    if (phase_ != SessionPhase::AwaitingResponse || !bytes ||
        size != 3 || bytes[0] != 1 || bytes[1] != 1 ||
        bytes[2] != 0) return false;
    commandAckSeen_ = true;
    return true;
  }

  // A future BLE callback should promptly call nextWrite()/onWriteResult()
  // for each fragment ACK, before processing more DATA notifications.
  bool onDataFragment(const uint8_t* bytes, size_t size) {
    if (phase_ != SessionPhase::AwaitingResponse || ackPending_ ||
        promptSeen_) { fail("Unexpected or overlapping DATA"); return false; }
    if (!bytes || size == 0 || size > 255) {
      fail("Invalid DATA fragment length");
      return false;
    }
    ackPending_ = true;
    ackLength_ = static_cast<uint8_t>(size);
    const bool done = response_.appendEncoded(bytes, size);
    if (response_.failed()) {
      fail("Response framing/length error");
      return false;
    }
    if (done) {
      if (response_.size() < 2) {
        fail("Empty prompt response");
        return false;
      }
      promptSeen_ = true;
    }
    return true; // returns success of acceptance; not completion
  }

  void poll(uint32_t nowMs) {
    // unsigned subtraction is rollover-safe for intervals shorter than 2^31 ms
    if (phase_ == SessionPhase::AwaitingResponse &&
        static_cast<uint32_t>(nowMs - commandStarted_) >= kResponseTimeoutMs)
      fail("Command response/ACK timeout");
  }

 private:
  static const char* const* commands() {
    static const char* const sequence[kCommandCount] = {
      "LMI", "LMD", "LML0", "LMI", "LMD", "LMDP",
      "LMI", "LMX1", "LMX0"
    };
    return sequence;
  }
  void completeResponse() {
    // Advance ONLY after receiving a complete '>' response and writing its ACK.
    if (phase_ != SessionPhase::AwaitingResponse || !response_.complete()) {
      fail("Incomplete command response");
      return;
    }
    const char* reply = response_.text();
    memcpy(lastReply_, reply, response_.size() + 1);
    ++completed_;
    phase_ = completed_ == kCommandCount ? SessionPhase::Ready
                                         : SessionPhase::NeedCommand;
    promptSeen_ = false;
    response_.reset();
  }
  void fail(const char* message) {
    phase_ = SessionPhase::Failed;
    ackPending_ = false;
    promptSeen_ = false;
    lastError_ = message;
    response_.reset();
  }

  SessionPhase phase_;
  size_t completed_;
  bool ackPending_;
  uint8_t ackLength_;
  bool promptSeen_;
  bool commandAckSeen_;
  uint32_t commandStarted_;
  const char* lastError_;
  char lastReply_[256];
  ResponseBuffer response_;
};
} // namespace bluedriver
