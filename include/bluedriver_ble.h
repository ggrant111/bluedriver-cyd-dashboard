#pragma once
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>

// v0.3 read-only BLE reconnaissance. No OBD commands or characteristic writes.
class BlueDriverBle {
 public:
  static const int kMaxDevices = 6;
  struct Candidate {
    std::string name;
    std::string address;
    int rssi;
    bool likely;
  };
  BlueDriverBle();
  void begin();
  void scan(bool includeAll = false);
  bool connectNumber(int number); // visible choices 1..6
  bool pairExplicitly();          // only on user 'p' serial command
  void disconnect();
  bool connected() const;
  void poll();
  int count() const { return count_; }
  const Candidate& candidate(int i) const { return candidates_[i]; }
  const char* status() const { return status_; }
  int serviceCount() const { return services_; }
  int knownCount() const { return known_; }
 private:
  void setStatus(const char* message);
  void inspectGatt();
  Candidate candidates_[kMaxDevices];
  int count_;
  NimBLEClient* client_;
  char status_[48];
  int services_;
  int known_;
};
