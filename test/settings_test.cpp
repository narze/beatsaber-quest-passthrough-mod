// Host unit tests for Settings / LiveSettings. Run: ./test/run.sh
#include "settings.hpp"

#include <cassert>
#include <cstdio>
#include <thread>

using namespace passthrough;

static void sanitize_clamps_ranges() {
  Settings s;
  s.opacity = 1.7f;
  s.keyLow = -0.5f;
  s.keyHigh = 3.0f;
  s = Sanitize(s);
  assert(s.opacity == 1.0f);
  assert(s.keyLow == 0.0f);
  assert(s.keyHigh == 1.0f);
}

static void sanitize_keeps_key_ordered() {
  Settings s;
  s.keyLow = 0.2f;
  s.keyHigh = 0.1f;
  s = Sanitize(s);
  assert(s.keyLow < s.keyHigh); // smoothstep needs low < high
}

static void sanitize_keeps_valid_values() {
  Settings s;
  s.opacity = 0.4f;
  s.keyLow = 0.0f;
  s.keyHigh = 0.015f;
  Settings out = Sanitize(s);
  assert(out.opacity == 0.4f && out.keyLow == 0.0f && out.keyHigh == 0.015f);
}

static void live_settings_update_and_version() {
  LiveSettings live;
  auto v0 = live.Version();
  live.Update([](Settings &s) { s.enabled = false; });
  assert(!live.Get().enabled);
  assert(live.Version() == v0 + 1);
}

static void live_settings_update_sanitizes() {
  LiveSettings live;
  live.Update([](Settings &s) { s.opacity = 5.0f; });
  assert(live.Get().opacity == 1.0f);
}

static void live_settings_concurrent_reads() {
  LiveSettings live;
  std::thread writer([&] {
    for (int i = 0; i < 1000; i++) live.Update([i](Settings &s) { s.keyHigh = (i % 10) / 100.0f + 0.01f; });
  });
  for (int i = 0; i < 1000; i++) {
    Settings s = live.Get();
    assert(s.keyLow < s.keyHigh);
  }
  writer.join();
}

static void show_mask_is_off_by_default() {
  assert(!Settings{}.showMask); // debug view: never on unless asked for
}

int main() {
  show_mask_is_off_by_default();
  sanitize_clamps_ranges();
  sanitize_keeps_key_ordered();
  sanitize_keeps_valid_values();
  live_settings_update_and_version();
  live_settings_update_sanitizes();
  live_settings_concurrent_reads();
  std::puts("all settings tests passed");
}
