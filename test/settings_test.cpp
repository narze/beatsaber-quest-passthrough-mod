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

static void quick_toggle_is_on_by_default() {
  assert(Settings{}.quickToggle); // A+B in songs
}

static void show_mask_is_off_by_default() {
  assert(!Settings{}.showMask); // debug view: never on unless asked for
}

static void bloom_fog_rules() {
  Settings s; // passthrough on, environment hidden, distance fog off
  assert(!WantBloomFog(true, s));
  s.distanceFog = true;
  assert(WantBloomFog(true, s));
  assert(!WantBloomFog(false, s)); // never on when the game has it off
  s.distanceFog = false;
  s.hideEnvironment = false;
  assert(WantBloomFog(true, s));
  s.hideEnvironment = true;
  s.enabled = false;
  assert(WantBloomFog(true, s));
}

static void black_light_texture_rules() {
  Settings s; // passthrough on, both environments hidden
  assert(BlackOutLightTexture(s, true));
  assert(BlackOutLightTexture(s, false));
  s.hideEnvironment = false;
  assert(!BlackOutLightTexture(s, true)); // song environment shown: keep its light
  assert(BlackOutLightTexture(s, false));
  s.hideMenuEnvironment = false;
  assert(!BlackOutLightTexture(s, false));
  s.hideEnvironment = true;
  s.enabled = false;
  assert(!BlackOutLightTexture(s, true));
}

static void pause_panel_pose_defaults() {
  PanelPose p = Settings{}.pausePanel; // left of the pause menu, turned to the player
  assert(p.x < 0.0f && p.y > 1.0f && p.z > 1.0f);
  float len = p.qx * p.qx + p.qy * p.qy + p.qz * p.qz + p.qw * p.qw;
  assert(len > 0.99f && len < 1.01f);
}

static void sanitize_keeps_pause_panel_reachable() {
  Settings s;
  s.pausePanel.x = 50.0f;
  s.pausePanel.y = -3.0f;
  s.pausePanel.z = -50.0f;
  s = Sanitize(s);
  assert(s.pausePanel.x == 3.0f && s.pausePanel.y == 0.0f && s.pausePanel.z == -3.0f);
}

static void sanitize_normalizes_pause_panel_rotation() {
  Settings s;
  s.pausePanel.qx = 0.0f;
  s.pausePanel.qy = 0.0f;
  s.pausePanel.qz = 0.0f;
  s.pausePanel.qw = 2.0f;
  s = Sanitize(s);
  assert(s.pausePanel.qw == 1.0f);
  s.pausePanel.qw = 0.0f; // not a rotation: back to the default
  s = Sanitize(s);
  assert(s.pausePanel.qy == PanelPose{}.qy && s.pausePanel.qw == PanelPose{}.qw);
}

int main() {
  pause_panel_pose_defaults();
  sanitize_keeps_pause_panel_reachable();
  sanitize_normalizes_pause_panel_rotation();
  black_light_texture_rules();
  bloom_fog_rules();
  show_mask_is_off_by_default();
  quick_toggle_is_on_by_default();
  sanitize_clamps_ranges();
  sanitize_keeps_key_ordered();
  sanitize_keeps_valid_values();
  live_settings_update_and_version();
  live_settings_update_sanitizes();
  live_settings_concurrent_reads();
  std::puts("all settings tests passed");
}
