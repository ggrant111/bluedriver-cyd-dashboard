#include <Arduino.h>
#include <TFT_eSPI.h>
#include "obd_pids.h"

// v0.1.0: only a visibly marked DEMO dashboard; no BlueDriver link yet.
// Real BLE communication and signal freshness arrive in later milestones.
static TFT_eSPI tft;
static constexpr uint16_t BG = 0x0841;
static constexpr uint16_t PANEL = 0x18E3;
static constexpr uint16_t WHITE = 0xFFFF;
static constexpr uint16_t MUTED = 0x94B2;
static constexpr uint16_t ACCENT = 0x05DF;
static constexpr uint16_t DEMO = 0xFD20;
static constexpr uint8_t BACKLIGHT_PIN = 21;

void tile(int x, int y, const char* label) {
  tft.fillRoundRect(x, y, 148, 83, 7, PANEL);
  tft.setTextColor(MUTED, PANEL);
  tft.setTextFont(2);
  tft.drawString(label, x + 10, y + 7, 2);
}

void value(int x, int y, const char* number, const char* units, uint16_t color) {
  tft.fillRect(x + 9, y + 32, 130, 41, PANEL); // partial redraw: no flicker
  tft.setTextColor(color, PANEL);
  tft.drawString(number, x + 10, y + 31, 4);
  tft.setTextColor(MUTED, PANEL);
  tft.drawRightString(units, x + 139, y + 56, 2);
}

void drawChrome() {
  tft.fillScreen(BG);
  tft.setTextColor(WHITE, BG);
  tft.drawString("BRONCO  /  OBD-II", 9, 7, 2);
  tft.fillRoundRect(250, 6, 61, 21, 5, DEMO);
  tft.setTextColor(0x0000, DEMO);
  tft.drawCentreString("DEMO", 280, 9, 2);
  tft.drawFastHLine(9, 33, 302, 0x3A49);
  tile(9, 41, "ENGINE RPM");
  tile(163, 41, "SPEED");
  tile(9, 132, "COOLANT");
  tile(163, 132, "ECU VOLTAGE");
  tft.setTextColor(MUTED, BG);
  tft.drawString("BlueDriver: NOT CONNECTED", 9, 224, 2);
}

void updateDemo() {
  static unsigned tick = 0;
  ++tick;
  // Intentional simulation. Never feed this path into the eventual live gauges.
  const int rpm = 870 + static_cast<int>((tick % 15) * 32);
  const int mph = 0;
  const int coolantF = 187 + static_cast<int>(tick % 5);
  char buffer[20];
  snprintf(buffer, sizeof(buffer), "%d", rpm);
  value(9, 41, buffer, "rpm", ACCENT);
  snprintf(buffer, sizeof(buffer), "%d", mph);
  value(163, 41, buffer, "mph", WHITE);
  snprintf(buffer, sizeof(buffer), "%d", coolantF);
  value(9, 132, buffer, "F", WHITE);
  value(163, 132, tick % 2 ? "13.9" : "13.8", "V", WHITE);
}

void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  tft.init();
  tft.setRotation(1); // 320 x 240 landscape for typical ILI9341 CYD
  drawChrome();
  updateDemo();
  Serial.println("[BlueDriver-CYD v0.1.0] DEMO ONLY; BLE not implemented.");
  Serial.println("[Display] Confirm board driver/color/touch before hardware use.");
}

void loop() {
  static uint32_t lastDrawMs = 0;
  const uint32_t now = millis();
  if (now - lastDrawMs >= 650U) {
    lastDrawMs = now;
    updateDemo();
  }
  delay(10);
}
