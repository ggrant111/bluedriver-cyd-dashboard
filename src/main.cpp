#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include <string.h>
#include "bluedriver_ble.h"
#include "dashboard_model.h"
#include "dashboard_navigation.h"

// v0.3.1: browser/CYD dashboard simulator, NOT real vehicle telemetry.
// BLE v0.3 discovery remains read-only, opt-in, and isolated from demo values.
static TFT_eSPI tft;
static BlueDriverBle ble;
static dash::Model model;
static nav::Screen screen = nav::Screen::Dashboard;
static Preferences preferences;

// Common ESP32-2432S028R XPT2046 uses separate VSPI: CLK25 MISO39 MOSI32 CS33 IRQ36.
// Many CYD clones differ: touch orientation/calibration MUST be verified physically.
static SPIClass touchBus(VSPI);
static XPT2046_Touchscreen touch(33, 36);
static bool touchAvailable = false;
static bool touchDebug = false;
static bool touchHeld = false;
static uint32_t lastTapMs = 0;
static uint32_t lastPaintMs = 0;
static char lastBleStatus[48] = "";
static constexpr uint16_t BG = 0x0841;
static constexpr uint16_t PANEL = 0x18E3;
static constexpr uint16_t WHITE = 0xFFFF;
static constexpr uint16_t MUTED = 0xA534;
static constexpr uint16_t TEAL = 0x04B1;
static constexpr uint16_t ORANGE = 0xFD20;
static constexpr uint16_t RED = 0xF9A7;
static constexpr uint8_t BACKLIGHT = 21;

struct Rect { int16_t x, y, w, h; };
static Rect gaugeRect(uint8_t index) {
  if (model.layout() == dash::Layout::Grid) {
    return Rect{static_cast<int16_t>(9 + (index % 2) * 154),
                static_cast<int16_t>(40 + (index / 2) * 84), 148, 80};
  }
  if (index == 0) return Rect{9, 40, 302, 88};
  return Rect{static_cast<int16_t>(9 + (index - 1) * 103), 134, 96, 70};
}

static void navBar(const char* a, const char* b, const char* c, const char* d) {
  const char* labels[4] = {a, b, c, d};
  tft.fillRect(0, 210, 320, 30, BG);
  for (int i = 0; i < 4; ++i) {
    tft.fillRoundRect(i * 80 + 3, 213, 74, 24, 4, PANEL);
    tft.setTextColor(WHITE, PANEL);
    tft.drawCentreString(labels[i], i * 80 + 40, 218, 2);
  }
}
static void header(const char* title, const char* badge) {
  tft.fillScreen(BG);
  tft.setTextColor(WHITE, BG);
  tft.drawString(title, 9, 8, 2);
  tft.fillRoundRect(245, 6, 66, 23, 4, ORANGE);
  tft.setTextColor(0x0000, ORANGE);
  tft.drawCentreString(badge, 278, 11, 2);
  tft.drawFastHLine(9, 34, 302, 0x39C7);
}
static const char* scenarioName() {
  switch (model.scenario()) {
    case dash::Scenario::Idle: return "IDLE";
    case dash::Scenario::Cruise: return "CRUISE";
    default: return "ACCEL";
  }
}
static void paintGauge(uint8_t index, uint32_t nowMs, bool frame) {
  const Rect r = gaugeRect(index);
  const dash::Metric metric = model.metric(index);
  const dash::Sample& s = model.sample(metric);
  const dash::Freshness state = model.freshness(metric, nowMs);
  if (frame) {
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, PANEL);
    tft.setTextColor(MUTED, PANEL);
    tft.drawString(dash::label(metric), r.x + 7, r.y + 5, r.w < 100 ? 1 : 2);
  }
  // Redraw only the value portion; no full-screen refresh/flicker.
  tft.fillRect(r.x + 4, r.y + 27, r.w - 8, r.h - 32, PANEL);
  const uint16_t color = state == dash::Freshness::Stale ? RED :
                         state == dash::Freshness::Delayed ? ORANGE :
                         index == 0 ? TEAL : WHITE;
  tft.setTextColor(color, PANEL);
  char num[24] = "--";
  if (s.valid) {
    const float converted = dash::convert(metric, s.raw, model.units());
    snprintf(num, sizeof(num), dash::decimals(metric) ? "%.1f" : "%.0f",
             converted);
  }
  const int font = (r.w > 100 && r.h > 72) ? 4 : 2;
  tft.drawString(num, r.x + 7, r.y + 29, font);
  tft.setTextColor(MUTED, PANEL);
  tft.drawRightString(dash::unit(metric, model.units()),
                      r.x + r.w - 6, r.y + r.h - 20, 1);
  const char* status = state == dash::Freshness::Stale ? "STALE" :
                       state == dash::Freshness::Delayed ? "LAG" :
                       state == dash::Freshness::Missing ? "WAIT" : "SIM";
  tft.setTextColor(color, PANEL);
  tft.drawRightString(status, r.x + r.w - 6, r.y + r.h - 10, 1);
}
static void renderDashboard(bool full = true) {
  const uint32_t nowMs = millis();
  if (full) {
    header("BRONCO  /  OBD-II", "DEMO");
    for (uint8_t i = 0; i < 4; ++i) paintGauge(i, nowMs, true);
    navBar("PAGE", "BLE", "SETUP",
           model.layout() == dash::Layout::Grid ? "FOCUS" : "GRID");
  } else {
    for (uint8_t i = 0; i < 4; ++i) paintGauge(i, nowMs, false);
  }
}
static void settingsTile(int x, int y, const char* label, const char* val) {
  tft.fillRoundRect(x, y, 148, 80, 6, PANEL);
  tft.setTextColor(MUTED, PANEL);
  tft.drawString(label, x + 9, y + 8, 2);
  tft.setTextColor(WHITE, PANEL);
  tft.drawString(val, x + 9, y + 37, 2);
}
static void renderSettings() {
  header("SIMULATOR SETUP", "DEMO");
  settingsTile(9, 40, "UNITS", model.units() == dash::Units::Imperial ? "MPH / F" : "KMH / C");
  settingsTile(163, 40, "LAYOUT", model.layout() == dash::Layout::Grid ? "2 X 2 GRID" : "FOCUS");
  settingsTile(9, 124, "SCENARIO", scenarioName());
  settingsTile(163, 124, "SIM DATA", model.paused() ? "PAUSED" : "RUNNING");
  navBar("DASH", "BLE", "PAGE", "RESET");
}
static void renderBluetooth() {
  header("BLUEDRIVER BLE", "TEST");
  tft.setTextColor(ble.connected() ? TEAL : MUTED, BG);
  tft.drawString(ble.status(), 9, 38, 2);
  for (int i = 0; i < ble.count() && i < 6; ++i) {
    const auto& d = ble.candidate(i);
    const int y = 58 + i * 25;
    tft.fillRoundRect(9, y, 302, 23, 3, PANEL);
    char left[36];
    snprintf(left, sizeof(left), "%d) %.19s", i + 1, d.name.c_str());
    tft.setTextColor(d.likely ? WHITE : MUTED, PANEL);
    tft.drawString(left, 15, y + 3, 2);
    char strength[18];
    snprintf(strength, sizeof(strength), "%d dBm", d.rssi);
    tft.drawRightString(strength, 305, y + 3, 2);
  }
  tft.setTextColor(MUTED, BG);
  tft.drawString("BLE discovery only; no vehicle reads", 9, 203, 1);
  navBar("DASH", "SCAN", "ALL", "SETUP");
  snprintf(lastBleStatus, sizeof(lastBleStatus), "%s", ble.status());
}
static void render() {
  if (screen == nav::Screen::Dashboard) renderDashboard();
  else if (screen == nav::Screen::Settings) renderSettings();
  else renderBluetooth();
}
static void saveConfig() {
  preferences.putUChar("units", static_cast<uint8_t>(model.units()));
  preferences.putUChar("layout", static_cast<uint8_t>(model.layout()));
  preferences.putUChar("page", model.page());
  preferences.putUChar("scenario", static_cast<uint8_t>(model.scenario()));
  for (uint8_t p = 0; p < dash::kPages; ++p)
    for (uint8_t s = 0; s < dash::kSlots; ++s) {
      char key[8];
      snprintf(key, sizeof(key), "g%d%d", p, s);
      preferences.putUChar(key, static_cast<uint8_t>(model.metricAt(p, s)));
    }
}
static void loadConfig() {
  model.setUnits(preferences.getUChar("units", 0) == 1 ?
                 dash::Units::Metric : dash::Units::Imperial);
  model.setLayout(preferences.getUChar("layout", 0) == 1 ?
                  dash::Layout::Focus : dash::Layout::Grid);
  model.setPage(preferences.getUChar("page", 0));
  model.setScenario(static_cast<dash::Scenario>(
                    preferences.getUChar("scenario", 0)));
  for (uint8_t p = 0; p < dash::kPages; ++p)
    for (uint8_t s = 0; s < dash::kSlots; ++s) {
      char key[8];
      snprintf(key, sizeof(key), "g%d%d", p, s);
      const uint8_t index = preferences.getUChar(
          key, static_cast<uint8_t>(dash::Model::defaultMetric(p, s)));
      if (index < static_cast<uint8_t>(dash::Metric::Count))
        model.chooseMetric(p, s, static_cast<dash::Metric>(index));
    }
}
static void scanBle(bool all) {
  screen = nav::Screen::Bluetooth;
  tft.fillScreen(BG);
  tft.setTextColor(WHITE, BG);
  tft.drawString(all ? "Scanning ALL nearby BLE..." : "Scanning BlueDriver...",
                 12, 70, 2);
  ble.scan(all); // blocking, explicit request, never auto-connect
  renderBluetooth();
}
static void perform(nav::Action a) {
  using nav::Action;
  if (a == Action::None) return;
  if (a >= Action::Gauge0 && a <= Action::Gauge3) {
    if (screen == nav::Screen::Dashboard) {
      model.cycleMetric(static_cast<uint8_t>(a) - static_cast<uint8_t>(Action::Gauge0));
      saveConfig();
    }
  } else if (a >= Action::Select1 && a <= Action::Select6) {
    if (screen == nav::Screen::Bluetooth)
      ble.connectNumber(static_cast<int>(a) - static_cast<int>(Action::Select1) + 1);
  } else {
    switch (a) {
      case Action::Dashboard: screen = nav::Screen::Dashboard; break;
      case Action::Bluetooth: screen = nav::Screen::Bluetooth; break;
      case Action::Settings: screen = nav::Screen::Settings; break;
      case Action::NextPage: model.nextPage(); saveConfig(); break;
      case Action::ToggleUnits: model.toggleUnits(); saveConfig(); break;
      case Action::ToggleLayout: model.toggleLayout(); saveConfig(); break;
      case Action::NextScenario: model.nextScenario(); saveConfig(); break;
      case Action::TogglePause: model.togglePause(); break;
      case Action::ResetSettings: model.resetLayout(); saveConfig(); break;
      case Action::Scan: scanBle(false); return;
      case Action::ScanAll: scanBle(true); return;
      default: break;
    }
  }
  render();
}
static void help() {
  Serial.println("\n[UI] BlueDriver CYD v0.3.1 — DEMO SIMULATOR");
  Serial.println("d dashboard | b BLE | g settings | n next page");
  Serial.println("u units | l layout | c scenario | f freeze/resume | r reset");
  Serial.println("1..4 cycles corresponding gauge (only in Dashboard)");
  Serial.println("s BlueDriver scan | a all BLE scan (only in BLE view)");
  Serial.println("1..6 connect candidate (only in BLE view) | p bond | x disconnect");
  Serial.println("t toggle raw touch debug | h help");
  Serial.println("Tap gauges to cycle metric. Bottom nav opens pages/settings/BLE.");
  Serial.println("All gauges are SIMULATED; no OBD commands are transmitted.");
}
static void serialCommand(char c) {
  if (c == '\n' || c == '\r' || c == ' ') return;
  if (c == 'h' || c == '?') { help(); return; }
  if (c == 'd') { perform(nav::Action::Dashboard); return; }
  if (c == 'b') { perform(nav::Action::Bluetooth); return; }
  if (c == 'g') { perform(nav::Action::Settings); return; }
  if (c == 'n') { perform(nav::Action::NextPage); return; }
  if (c == 'u') { perform(nav::Action::ToggleUnits); return; }
  if (c == 'l') { perform(nav::Action::ToggleLayout); return; }
  if (c == 'c') { perform(nav::Action::NextScenario); return; }
  if (c == 'f') { perform(nav::Action::TogglePause); return; }
  if (c == 'r') { perform(nav::Action::ResetSettings); return; }
  if (c == 't') { touchDebug = !touchDebug; Serial.printf("[TOUCH] debug=%d\n", touchDebug); return; }
  if (c == 'x') { ble.disconnect(); if (screen == nav::Screen::Bluetooth) render(); return; }
  if (c == 'p') { ble.pairExplicitly(); if (screen == nav::Screen::Bluetooth) render(); return; }
  if (c == 's' && screen == nav::Screen::Bluetooth) { scanBle(false); return; }
  if (c == 'a' && screen == nav::Screen::Bluetooth) { scanBle(true); return; }
  if (c >= '1' && c <= '6') {
    if (screen == nav::Screen::Bluetooth) {
      ble.connectNumber(c - '0');
      render();
    } else if (screen == nav::Screen::Dashboard && c <= '4') {
      perform(static_cast<nav::Action>(
          static_cast<int>(nav::Action::Gauge0) + (c - '1')));
    }
    return;
  }
  Serial.printf("[UI] Unknown command '%c' — 'h' for help\n", c);
}
static void pollTouch() {
  if (!touchAvailable) return;
  const bool pressed = touch.touched();
  if (!pressed) { touchHeld = false; return; }
  if (touchHeld || millis() - lastTapMs < 250U) return;
  const TS_Point point = touch.getPoint();
  touchHeld = true;
  lastTapMs = millis();
  // APPROXIMATE LANDSCAPE CALIBRATION; adjust after raw corner measurements.
  // Default mapping may be mirrored/rotated on different CYD revisions.
  int x = map(point.x, 240, 3800, 0, 319);
  int y = map(point.y, 280, 3750, 0, 239);
  x = constrain(x, 0, 319);
  y = constrain(y, 0, 239);
  if (touchDebug)
    Serial.printf("[TOUCH] raw=(%d,%d,%d) mapped=(%d,%d)\n",
                  point.x, point.y, point.z, x, y);
  perform(nav::actionAt(screen, model.layout(), x, y));
}
void setup() {
  Serial.begin(115200);
  pinMode(BACKLIGHT, OUTPUT);
  digitalWrite(BACKLIGHT, HIGH);
  tft.init();
  tft.setRotation(1);
  preferences.begin("bdash", false);
  loadConfig();
  model.tick(millis());
  Serial.printf("[BOOT] BlueDriver-CYD 0.3.1 OFFLINE DEMO free_heap=%u\n",
                ESP.getFreeHeap());
  ble.begin();
  // No scan on boot. Users may test dashboard and touch without an OBD adapter.
  touchBus.begin(25, 39, 32, 33);
  touchAvailable = touch.begin(touchBus);
  touch.setRotation(0);
  Serial.printf("[TOUCH] initialized=%d; calibration UNVERIFIED, press t for raw\n",
                touchAvailable);
  help();
  render();
}
void loop() {
  while (Serial.available()) serialCommand(static_cast<char>(Serial.read()));
  pollTouch();
  ble.poll();
  const uint32_t now = millis();
  const bool sampled = model.tick(now);
  if (screen == nav::Screen::Dashboard &&
      (sampled || static_cast<uint32_t>(now - lastPaintMs) >= 450U)) {
    lastPaintMs = now;
    renderDashboard(false);
  }
  if (screen == nav::Screen::Bluetooth &&
      strcmp(ble.status(), lastBleStatus) != 0) renderBluetooth();
  delay(12);
}
