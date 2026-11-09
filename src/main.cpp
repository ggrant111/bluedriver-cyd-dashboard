#include <Arduino.h>
#include <TFT_eSPI.h>
#include <string.h>
#include "bluedriver_ble.h"
#include "obd_pids.h"

// v0.3.0 EXPERIMENTAL: read-only BlueDriver discovery/GATT reconnaissance.
// Simulated gauge values are ALWAYS marked DEMO, never connected to BLE data.
static TFT_eSPI tft;
static BlueDriverBle ble;
static bool showBle = true;
static uint32_t lastDemoMs = 0;
static char lastBleState[48] = "";
static constexpr uint16_t BG = 0x0841;
static constexpr uint16_t PANEL = 0x18E3;
static constexpr uint16_t WHITE = 0xFFFF;
static constexpr uint16_t MUTED = 0x94B2;
static constexpr uint16_t ACCENT = 0x05DF;
static constexpr uint16_t ORANGE = 0xFD20;
static constexpr uint8_t BACKLIGHT_PIN = 21;

void tile(int x, int y, const char* label) {
  tft.fillRoundRect(x, y, 148, 83, 7, PANEL);
  tft.setTextColor(MUTED, PANEL);
  tft.drawString(label, x + 10, y + 7, 2);
}

void value(int x, int y, const char* number, const char* units, uint16_t color) {
  tft.fillRect(x + 9, y + 32, 130, 41, PANEL);
  tft.setTextColor(color, PANEL);
  tft.drawString(number, x + 10, y + 31, 4);
  tft.setTextColor(MUTED, PANEL);
  tft.drawRightString(units, x + 139, y + 56, 2);
}

void drawDemo() {
  tft.fillScreen(BG);
  tft.setTextColor(WHITE, BG);
  tft.drawString("BRONCO / OBD-II", 9, 7, 2);
  tft.fillRoundRect(250, 6, 61, 21, 5, ORANGE);
  tft.setTextColor(0x0000, ORANGE);
  tft.drawCentreString("DEMO", 280, 9, 2);
  tft.drawFastHLine(9, 33, 302, 0x3A49);
  tile(9, 41, "ENGINE RPM");
  tile(163, 41, "SPEED");
  tile(9, 132, "COOLANT");
  tile(163, 132, "ECU VOLTAGE");
  tft.setTextColor(MUTED, BG);
  tft.drawString("SIMULATED / NO LIVE OBD DATA", 9, 224, 2);
}

void updateDemo() {
  static unsigned tick = 0;
  ++tick;
  const int rpm = 870 + static_cast<int>((tick % 15) * 32);
  const int coolantF = 187 + static_cast<int>(tick % 5);
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%d", rpm);
  value(9, 41, buffer, "rpm", ACCENT);
  value(163, 41, "0", "mph", WHITE);
  snprintf(buffer, sizeof(buffer), "%d", coolantF);
  value(9, 132, buffer, "F", WHITE);
  value(163, 132, tick % 2 ? "13.9" : "13.8", "V", WHITE);
}

void drawBle() {
  showBle = true;
  tft.fillScreen(BG);
  tft.setTextColor(WHITE, BG);
  tft.drawString("BLUEDRIVER / BLE", 9, 6, 2);
  tft.setTextColor(ORANGE, BG);
  tft.drawRightString("v0.3 TEST", 311, 6, 2);
  tft.drawFastHLine(9, 30, 302, 0x3A49);
  tft.setTextColor(ble.connected() ? ACCENT : MUTED, BG);
  tft.drawString(ble.status(), 9, 34, 2);
  const int count = ble.count();
  for (int i = 0; i < count; ++i) {
    const BlueDriverBle::Candidate& dev = ble.candidate(i);
    const int y = 58 + i * 25;
    tft.fillRoundRect(9, y, 302, 23, 3, PANEL);
    char left[36];
    // Names are not trusted: truncate before drawing into the 320px UI.
    snprintf(left, sizeof(left), "%d) %.19s", i + 1, dev.name.c_str());
    tft.setTextColor(dev.likely ? WHITE : MUTED, PANEL);
    tft.drawString(left, 15, y + 3, 2);
    char rssi[15];
    snprintf(rssi, sizeof(rssi), "%d dBm", dev.rssi);
    tft.drawRightString(rssi, 305, y + 3, 2);
  }
  if (ble.connected()) {
    char summary[40];
    snprintf(summary, sizeof(summary), "GATT services: %d  known: %d/4",
             ble.serviceCount(), ble.knownCount());
    tft.setTextColor(ACCENT, BG);
    tft.drawString(summary, 10, 210, 2);
  } else {
    tft.setTextColor(MUTED, BG);
    tft.drawString("No vehicle commands are sent.", 10, 210, 2);
  }
  tft.setTextColor(MUTED, BG);
  tft.drawString("Serial: s scan  1-6 connect  p pair  h help", 9, 225, 1);
  snprintf(lastBleState, sizeof(lastBleState), "%s", ble.status());
}

void help() {
  Serial.println("\n=== BlueDriver-CYD v0.3 BLE test ===");
  Serial.println("s = scan for BlueDriver-name matches (6 seconds)");
  Serial.println("a = show all BLE advertisements (privacy: keep logs private)");
  Serial.println("1..6 = explicitly connect to a listed device and enumerate GATT");
  Serial.println("p = explicit Just Works bonding attempt on connected device");
  Serial.println("x = disconnect   b = BLE screen   d = DEMO gauge screen");
  Serial.println("h = help");
  Serial.println("Only GATT discovery + optional bonding. NO OBD requests or writes.");
  Serial.println("Use 115200 baud; scanner powered by parked vehicle ACC/ignition.");
}

void handleSerial(char command) {
  if (command == '\r' || command == '\n' || command == ' ') return;
  if (command == 'h' || command == '?') { help(); return; }
  if (command == 'd') { showBle = false; drawDemo(); updateDemo(); return; }
  if (command == 'b') { drawBle(); return; }
  if (command == 'x') { ble.disconnect(); drawBle(); return; }
  if (command == 'p') { ble.pairExplicitly(); drawBle(); return; }
  if (command >= '1' && command <= '6') {
    ble.connectNumber(command - '0');
    drawBle();
    return;
  }
  if (command == 's' || command == 'a') {
    showBle = true;
    tft.fillScreen(BG);
    tft.setTextColor(WHITE, BG);
    tft.drawString("Scanning Bluetooth...", 12, 50, 2);
    ble.scan(command == 'a');
    drawBle();
    return;
  }
  Serial.printf("[UI] Unknown command '%c' (h for help)\n", command);
}

void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(BG);
  tft.setTextColor(WHITE, BG);
  tft.drawString("BlueDriver CYD  v0.3", 10, 20, 2);
  Serial.println("[BOOT] BlueDriver CYD v0.3.0 EXPERIMENTAL");
  Serial.printf("[BOOT] free_heap=%u\n", ESP.getFreeHeap());
  Serial.println("[BOOT] NOTE: physical TFT/touch revision remains unverified");
  ble.begin();
  help();
  drawBle();
  handleSerial('s'); // first discovery only, never auto-connect or auto-pair
}

void loop() {
  while (Serial.available()) handleSerial(static_cast<char>(Serial.read()));
  ble.poll();
  if (showBle && strcmp(lastBleState, ble.status()) != 0) drawBle();
  if (!showBle && millis() - lastDemoMs >= 650U) {
    lastDemoMs = millis();
    updateDemo();
  }
  delay(15);
}
