#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "dashboard_model.h"
#include "dashboard_navigation.h"
using namespace dash;
using namespace nav;
static void eq(float a, float b) { assert(fabsf(a - b) < .03f); }
int main() {
  Model m;
  assert(m.page() == 0 && m.layout() == Layout::Grid);
  assert(m.metric(0) == Metric::Rpm && m.metric(3) == Metric::Voltage);
  assert(m.freshness(Metric::Rpm, 0) == Freshness::Missing);
  assert(m.tick(0));
  assert(m.freshness(Metric::Rpm, 0) == Freshness::Fresh);
  assert(!m.tick(100));
  assert(m.tick(650));
  assert(m.freshness(Metric::Rpm, 2149) == Freshness::Fresh);
  assert(m.freshness(Metric::Rpm, 2150) == Freshness::Delayed);
  assert(m.freshness(Metric::Rpm, 4150) == Freshness::Stale);
  m.setPaused(true);
  assert(!m.tick(10000));
  assert(m.freshness(Metric::Rpm, 10000) == Freshness::Stale);
  m.setPaused(false);
  assert(m.tick(10000));
  assert(m.freshness(Metric::Rpm, 10000) == Freshness::Fresh);
  eq(convert(Metric::Speed, 100, Units::Imperial), 62.1371f);
  eq(convert(Metric::Speed, 100, Units::Metric), 100);
  eq(convert(Metric::Coolant, 100, Units::Imperial), 212);
  eq(convert(Metric::Coolant, 100, Units::Metric), 100);
  assert(decimals(Metric::Voltage) == 1);
  assert(decimals(Metric::Rpm) == 0);
  assert(m.sample(Metric::Speed).raw == 0);
  m.setScenario(Scenario::Cruise);
  assert(m.tick(10700));
  assert(m.sample(Metric::Speed).raw > 80);
  m.setScenario(Scenario::Accelerate);
  assert(m.tick(11400));
  assert(m.sample(Metric::Speed).raw > 0);
  m.nextPage(); assert(m.page() == 1);
  assert(m.metric(0) == Metric::Load);
  m.cycleMetric(0); assert(m.metric(0) == Metric::Throttle);
  m.chooseMetric(1,0,Metric::Fuel); assert(m.metric(0) == Metric::Fuel);
  m.chooseMetric(1,0,Metric::Count); assert(m.metric(0) == Metric::Fuel);
  m.toggleUnits(); assert(m.units() == Units::Metric);
  m.toggleLayout(); assert(m.layout() == Layout::Focus);
  m.resetLayout();
  assert(m.page() == 0 && m.units() == Units::Imperial);
  assert(m.layout() == Layout::Grid && !m.paused());
  assert(m.metric(0) == Metric::Rpm);
  Model wrap;
  assert(wrap.tick(0xFFFFFFF0u));
  assert(wrap.freshness(Metric::Rpm, 0x00000001u) == Freshness::Fresh);
  assert(wrap.freshness(Metric::Rpm, 0x00000F00u) == Freshness::Stale);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 20, 60) == Action::Gauge0);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 200, 60) == Action::Gauge1);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 20, 150) == Action::Gauge2);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 200, 150) == Action::Gauge3);
  assert(actionAt(Screen::Dashboard, Layout::Focus, 200, 60) == Action::Gauge0);
  assert(actionAt(Screen::Dashboard, Layout::Focus, 20, 155) == Action::Gauge1);
  assert(actionAt(Screen::Dashboard, Layout::Focus, 145, 155) == Action::Gauge2);
  assert(actionAt(Screen::Dashboard, Layout::Focus, 265, 155) == Action::Gauge3);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 20, 221) == Action::NextPage);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 120, 221) == Action::Bluetooth);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 198, 221) == Action::Settings);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 290, 221) == Action::ToggleLayout);
  assert(actionAt(Screen::Bluetooth, Layout::Grid, 90, 225) == Action::Scan);
  assert(actionAt(Screen::Bluetooth, Layout::Grid, 30, 60) == Action::Select1);
  assert(actionAt(Screen::Bluetooth, Layout::Grid, 30, 184) == Action::Select6);
  assert(actionAt(Screen::Settings, Layout::Grid, 20, 80) == Action::ToggleUnits);
  assert(actionAt(Screen::Settings, Layout::Grid, 200, 80) == Action::ToggleLayout);
  assert(actionAt(Screen::Settings, Layout::Grid, 20, 155) == Action::NextScenario);
  assert(actionAt(Screen::Settings, Layout::Grid, 200, 155) == Action::TogglePause);
  assert(actionAt(Screen::Dashboard, Layout::Grid, 319, 240) == Action::None);
  assert(actionAt(Screen::Dashboard, Layout::Grid, -1, 100) == Action::None);
  printf("PASS: synthetic telemetry, conversion, freshness, preferences, touch routing\n");
}
