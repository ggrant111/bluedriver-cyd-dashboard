#pragma once
// Platform-independent simulated dashboard model; never represents real OBD-II.
// Both CYD firmware and browser simulator mirror the documented contract.
#include <stddef.h>
#include <stdint.h>
#include "obd_pids.h"

namespace dash {
enum class Metric : uint8_t {
  Rpm, Speed, Coolant, Voltage, Load, Throttle, Intake, Fuel, Manifold, Count
};
enum class Units : uint8_t { Imperial, Metric };
enum class Layout : uint8_t { Grid, Focus };
enum class Scenario : uint8_t { Idle, Cruise, Accelerate };
enum class Freshness : uint8_t { Missing, Fresh, Delayed, Stale };
static const uint8_t kPages = 2;
static const uint8_t kSlots = 4;
static const uint32_t kUpdateMs = 650;
static const uint32_t kDelayedMs = 1500;
static const uint32_t kStaleMs = 3500;

struct Sample {
  float raw;
  uint32_t atMs;
  bool valid;
  Sample() : raw(0), atMs(0), valid(false) {}
};
inline const char* label(Metric metric) {
  switch (metric) {
    case Metric::Rpm: return "ENGINE RPM";
    case Metric::Speed: return "SPEED";
    case Metric::Coolant: return "COOLANT";
    case Metric::Voltage: return "ECU VOLT";
    case Metric::Load: return "ENGINE LOAD";
    case Metric::Throttle: return "THROTTLE";
    case Metric::Intake: return "INTAKE AIR";
    case Metric::Fuel: return "FUEL LEVEL";
    case Metric::Manifold: return "MAP ABS";
    default: return "UNKNOWN";
  }
}
inline const char* unit(Metric metric, Units units) {
  switch (metric) {
    case Metric::Rpm: return "rpm";
    case Metric::Speed: return units == Units::Imperial ? "mph" : "km/h";
    case Metric::Coolant:
    case Metric::Intake: return units == Units::Imperial ? "F" : "C";
    case Metric::Voltage: return "V";
    case Metric::Load:
    case Metric::Throttle:
    case Metric::Fuel: return "%";
    case Metric::Manifold: return "kPa";
    default: return "";
  }
}
inline float convert(Metric metric, float raw, Units units) {
  if (units == Units::Metric) return raw;
  if (metric == Metric::Speed) return obd::kmhToMph(raw);
  if (metric == Metric::Coolant || metric == Metric::Intake)
    return obd::cToF(raw);
  return raw;
}
inline unsigned decimals(Metric metric) {
  return metric == Metric::Voltage ? 1U : 0U;
}

class Model {
 public:
  Model() : units_(Units::Imperial), layout_(Layout::Grid),
            scenario_(Scenario::Idle), page_(0), paused_(false),
            started_(false), lastTickMs_(0) {
    for (uint8_t page = 0; page < kPages; ++page)
      for (uint8_t slot = 0; slot < kSlots; ++slot)
        selected_[page][slot] = defaultMetric(page, slot);
  }
  static Metric defaultMetric(uint8_t page, uint8_t slot) {
    static const Metric first[kSlots] = {
      Metric::Rpm, Metric::Speed, Metric::Coolant, Metric::Voltage
    };
    static const Metric second[kSlots] = {
      Metric::Load, Metric::Throttle, Metric::Intake, Metric::Fuel
    };
    return (page % kPages == 0 ? first : second)[slot % kSlots];
  }
  uint8_t page() const { return page_; }
  Units units() const { return units_; }
  Layout layout() const { return layout_; }
  Scenario scenario() const { return scenario_; }
  bool paused() const { return paused_; }
  void nextPage() { page_ = (page_ + 1) % kPages; }
  void setPage(uint8_t value) { page_ = value % kPages; }
  void toggleUnits() {
    units_ = units_ == Units::Imperial ? Units::Metric : Units::Imperial;
  }
  void setUnits(Units units) { units_ = units; }
  void toggleLayout() {
    layout_ = layout_ == Layout::Grid ? Layout::Focus : Layout::Grid;
  }
  void setLayout(Layout layout) { layout_ = layout; }
  void nextScenario() {
    scenario_ = static_cast<Scenario>((static_cast<uint8_t>(scenario_) + 1U) % 3U);
  }
  void setScenario(Scenario scenario) {
    if (static_cast<uint8_t>(scenario) < 3U) scenario_ = scenario;
  }
  void togglePause() { paused_ = !paused_; }
  void setPaused(bool paused) { paused_ = paused; }
  Metric metric(uint8_t slot) const { return selected_[page_][slot % kSlots]; }
  Metric metricAt(uint8_t page, uint8_t slot) const {
    return selected_[page % kPages][slot % kSlots];
  }
  void chooseMetric(uint8_t page, uint8_t slot, Metric metric) {
    if (static_cast<uint8_t>(metric) >= static_cast<uint8_t>(Metric::Count)) return;
    selected_[page % kPages][slot % kSlots] = metric;
  }
  void cycleMetric(uint8_t slot) {
    uint8_t index = static_cast<uint8_t>(metric(slot));
    selected_[page_][slot % kSlots] =
        static_cast<Metric>((index + 1U) % static_cast<uint8_t>(Metric::Count));
  }
  void resetLayout() {
    units_ = Units::Imperial;
    layout_ = Layout::Grid;
    scenario_ = Scenario::Idle;
    paused_ = false;
    page_ = 0;
    for (uint8_t page = 0; page < kPages; ++page)
      for (uint8_t slot = 0; slot < kSlots; ++slot)
        selected_[page][slot] = defaultMetric(page, slot);
  }
  const Sample& sample(Metric metric) const {
    return readings_[static_cast<uint8_t>(metric)];
  }
  Freshness freshness(Metric metric, uint32_t nowMs) const {
    const Sample& item = sample(metric);
    if (!item.valid) return Freshness::Missing;
    uint32_t age = static_cast<uint32_t>(nowMs - item.atMs);
    if (age >= kStaleMs) return Freshness::Stale;
    if (age >= kDelayedMs) return Freshness::Delayed;
    return Freshness::Fresh;
  }
  // Synthetic only: samples are generated deterministically and are never
  // populated by OBD/BLE callbacks. 'paused' intentionally creates stale data.
  bool tick(uint32_t nowMs) {
    if (paused_) return false;
    if (started_ && static_cast<uint32_t>(nowMs - lastTickMs_) < kUpdateMs)
      return false;
    started_ = true;
    lastTickMs_ = nowMs;
    const uint32_t step = nowMs / kUpdateMs;
    float rpm, speed, load, throttle, map;
    if (scenario_ == Scenario::Idle) {
      rpm = 800.f + 20.f * (step % 7);
      speed = 0.f;
      load = 14.f + 2.f * (step % 6);
      throttle = 11.f + (step % 4);
      map = 39.f + (step % 5);
    } else if (scenario_ == Scenario::Cruise) {
      rpm = 1700.f + 33.f * (step % 12);
      speed = 89.f + (step % 6);
      load = 38.f + 2.f * (step % 8);
      throttle = 23.f + (step % 9);
      map = 58.f + (step % 14);
    } else {
      rpm = 1200.f + 155.f * (step % 25);
      speed = 8.f + 5.f * (step % 21);
      load = 45.f + 2.f * (step % 22);
      throttle = 30.f + 3.f * (step % 22);
      map = 65.f + 3.f * (step % 20);
    }
    update(Metric::Rpm, rpm, nowMs);
    update(Metric::Speed, speed, nowMs);
    update(Metric::Coolant, 84.f + static_cast<float>(step % 11), nowMs);
    update(Metric::Voltage, 13.7f + 0.1f * (step % 3), nowMs);
    update(Metric::Load, load, nowMs);
    update(Metric::Throttle, throttle, nowMs);
    update(Metric::Intake, 24.f + static_cast<float>(step % 12), nowMs);
    update(Metric::Fuel, 73.f - static_cast<float>(step % 8), nowMs);
    update(Metric::Manifold, map, nowMs);
    return true;
  }
 private:
  void update(Metric metric, float raw, uint32_t nowMs) {
    Sample& value = readings_[static_cast<uint8_t>(metric)];
    value.raw = raw;
    value.atMs = nowMs;
    value.valid = true;
  }
  Metric selected_[kPages][kSlots];
  Sample readings_[static_cast<uint8_t>(Metric::Count)];
  Units units_;
  Layout layout_;
  Scenario scenario_;
  uint8_t page_;
  bool paused_;
  bool started_;
  uint32_t lastTickMs_;
};
} // namespace dash
