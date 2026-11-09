#pragma once
// Pure geometry routing for landscape 320x240 dashboard.
// Actual XPT2046 touch calibration must be verified on the physical CYD.
#include <stdint.h>
#include "dashboard_model.h"
namespace nav {
enum class Screen : uint8_t { Dashboard, Bluetooth, Settings };
enum class Action : uint8_t {
  None, NextPage, Dashboard, Bluetooth, Settings, ToggleLayout,
  ToggleUnits, NextScenario, TogglePause, ResetSettings, Scan, ScanAll,
  Gauge0, Gauge1, Gauge2, Gauge3,
  Select1, Select2, Select3, Select4, Select5, Select6
};
inline Action actionAt(Screen screen, dash::Layout layout, int x, int y) {
  if (x < 0 || x >= 320 || y < 0 || y >= 240) return Action::None;
  if (y >= 211) {
    const int segment = x / 80;
    if (screen == Screen::Dashboard) {
      switch (segment) {
        case 0: return Action::NextPage;
        case 1: return Action::Bluetooth;
        case 2: return Action::Settings;
        case 3: return Action::ToggleLayout;
      }
    } else if (screen == Screen::Bluetooth) {
      switch (segment) {
        case 0: return Action::Dashboard;
        case 1: return Action::Scan;
        case 2: return Action::ScanAll;
        case 3: return Action::Settings;
      }
    } else {
      switch (segment) {
        case 0: return Action::Dashboard;
        case 1: return Action::Bluetooth;
        case 2: return Action::NextPage;
        case 3: return Action::ResetSettings;
      }
    }
    return Action::None;
  }
  if (screen == Screen::Bluetooth) {
    if (y < 56) return Action::None;
    int i = (y - 58) / 25;
    if (y >= 58 && i >= 0 && i < 6 && (y - 58) % 25 < 23)
      return static_cast<Action>(static_cast<int>(Action::Select1) + i);
    return Action::None;
  }
  if (y < 37 || y > 203) return Action::None;
  if (screen == Screen::Settings) {
    if (y < 40 || (y >= 120 && y < 124)) return Action::None;
    if (x < 9 || x > 311 || (x >= 157 && x < 163)) return Action::None;
    const int i = (y < 120 ? 0 : 2) + (x < 160 ? 0 : 1);
    switch (i) {
      case 0: return Action::ToggleUnits;
      case 1: return Action::ToggleLayout;
      case 2: return Action::NextScenario;
      case 3: return Action::TogglePause;
    }
    return Action::None;
  }
  if (layout == dash::Layout::Focus) {
    if (x < 9 || x > 311) return Action::None;
    if (y <= 124) return Action::Gauge0;
    if (y < 132) return Action::None;
    int column = (x - 9) / 102;
    if (column > 2) column = 2;
    return static_cast<Action>(static_cast<int>(Action::Gauge1) + column);
  }
  if (x < 9 || x > 311 || (x >= 157 && x < 163)) return Action::None;
  if (y < 41 || (y >= 122 && y < 126)) return Action::None;
  int column = x < 160 ? 0 : 1;
  int row = y < 122 ? 0 : 1;
  return static_cast<Action>(static_cast<int>(Action::Gauge0) +
                              row * 2 + column);
}
}
