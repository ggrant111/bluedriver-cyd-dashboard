#include "bluedriver_ble.h"
#include "ble_identity.h"
#include <vector>

// NimBLE-Arduino pinned at 1.4.3, matching ESP32 Arduino 2.x toolchain.
BlueDriverBle::BlueDriverBle()
    : count_(0), client_(nullptr), services_(0), known_(0) {
  setStatus("Not started");
}
void BlueDriverBle::setStatus(const char* message) {
  snprintf(status_, sizeof(status_), "%s", message);
  Serial.printf("[BLE] state=%s\n", status_);
}
void BlueDriverBle::begin() {
  NimBLEDevice::init("BlueDriver-CYD");
  // BlueDriver research reports Just Works with NoInputNoOutput.
  // This only configures capability: bonding must be explicitly requested.
  NimBLEDevice::setSecurityAuth(true, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->setActiveScan(true);
  scanner->setInterval(100);
  scanner->setWindow(75);
  setStatus("Ready to scan");
}
void BlueDriverBle::scan(bool includeAll) {
  if (connected()) {
    setStatus("Disconnect first (x)");
    return;
  }
  disconnect();
  count_ = 0;
  services_ = 0;
  known_ = 0;
  setStatus("Scanning 6 seconds...");
  Serial.printf("[BLE] scan begin mode=%s\n",
                includeAll ? "ALL (manual diagnostics)" : "BlueDriver names");
  NimBLEScan* scanner = NimBLEDevice::getScan();
  scanner->clearResults();
  NimBLEScanResults results = scanner->start(6, false); // 1.x: duration seconds
  Serial.printf("[BLE] advertisements=%d\n", results.getCount());
  for (int i = 0; i < results.getCount(); ++i) {
    // NimBLE 1.4.3 returns NimBLEAdvertisedDevice by value (2.x returns pointer).
    auto device = results.getDevice(i);
    const std::string name = device.getName();
    bool likely = bleidentity::isLikelyBlueDriver(name.c_str());
    if (!likely && !includeAll) continue;
    if (count_ >= kMaxDevices) break;
    Candidate& c = candidates_[count_++];
    c.name = name.empty() ? "(unnamed)" : name;
    c.address = device.getAddress().toString();
    c.rssi = device.getRSSI();
    c.likely = likely;
    // Only print last MAC octets; do not commit unredacted device identifiers.
    const char* tail = c.address.size() >= 5 ?
                      c.address.c_str() + c.address.size() - 5 : "?????";
    Serial.printf("[BLE] %d name='%s' RSSI=%d id=**:**:**:**:%s %s\n",
                  count_, c.name.c_str(), c.rssi, tail,
                  likely ? "BlueDriver candidate" : "unverified");
  }
  scanner->clearResults();
  setStatus(count_ ? "Select 1-6 to connect" : "No devices. 's' retry");
}
bool BlueDriverBle::connected() const {
  return client_ && client_->isConnected();
}
void BlueDriverBle::inspectGatt() {
  known_ = 0;
  services_ = 0;
  if (!connected()) return;
  setStatus("Discovering GATT...");
  // Discover all UUIDs once for diagnostics. Later releases can target
  // known services directly to save heap and reduce BLE round-trips.
  std::vector<NimBLERemoteService*>* services = client_->getServices(true);
  if (!services) {
    setStatus("GATT discovery failed");
    return;
  }
  for (auto* service : *services) {
    if (!service || !connected()) break;
    ++services_;
    Serial.printf("[GATT] service=%s\n", service->getUUID().toString().c_str());
    std::vector<NimBLERemoteCharacteristic*>* chars =
        service->getCharacteristics(true);
    if (!chars) continue;
    for (auto* characteristic : *chars) {
      if (!characteristic || !connected()) break;
      const std::string uuid = characteristic->getUUID().toString();
      const char* label = bleidentity::knownCharacteristic(uuid.c_str());
      if (label) ++known_;
      Serial.printf(
          "[GATT] char=%s known=%s read=%d write=%d writeNR=%d notify=%d indicate=%d\n",
          uuid.c_str(), label ? label : "-",
          characteristic->canRead(), characteristic->canWrite(),
          characteristic->canWriteNoResponse(), characteristic->canNotify(),
          characteristic->canIndicate());
    }
  }
  Serial.printf("[GATT] complete services=%d known_BlueDriver_characteristics=%d/4\n",
                services_, known_);
  setStatus(connected() ? "GATT read only; 'p' pair" : "Disconnected in GATT");
}
bool BlueDriverBle::connectNumber(int number) {
  if (number < 1 || number > count_) {
    setStatus("Invalid selection");
    return false;
  }
  disconnect();
  setStatus("Connecting...");
  const Candidate& selected = candidates_[number - 1];
  Serial.printf("[BLE] connect choice=%d name='%s'\n",
                number, selected.name.c_str());
  client_ = NimBLEDevice::createClient();
  if (!client_) {
    setStatus("Client allocation failed");
    return false;
  }
  // Do not assume the service UUID; use address remembered from scan.
  const NimBLEAddress address(selected.address);
  if (!client_->connect(address)) {
    setStatus("Connection failed");
    NimBLEDevice::deleteClient(client_);
    client_ = nullptr;
    return false;
  }
  Serial.println("[BLE] link connected; enumerating services (no writes)");
  inspectGatt();
  return connected();
}
bool BlueDriverBle::pairExplicitly() {
  if (!connected()) {
    setStatus("Connect before pairing");
    return false;
  }
  setStatus("Requesting BLE bond...");
  const bool ok = client_->secureConnection();
  Serial.printf("[BLE] secureConnection result=%s\n", ok ? "OK" : "FAILED");
  setStatus(ok ? "BLE secured; no OBD yet" : "Pair failed; check serial");
  return ok;
}
void BlueDriverBle::disconnect() {
  if (client_) {
    if (client_->isConnected()) client_->disconnect();
    NimBLEDevice::deleteClient(client_);
    client_ = nullptr;
  }
}
void BlueDriverBle::poll() {
  if (client_ && !client_->isConnected()) {
    disconnect();
    setStatus("Link lost; 's' to retry");
  }
}
