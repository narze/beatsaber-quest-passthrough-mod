#pragma once

#include "xr_logic.hpp"

namespace passthrough {

struct Settings {
  LayerPlacement placement = LayerPlacement::Underlay;
  float opacity = 1.0f;    // passthrough texture opacity
  bool sourceAlpha = true; // blend the game over passthrough with its alpha
  // Replace the game's alpha (glow strength in Beat Saber) with a key of the
  // pixel brightness: alpha = smoothstep(keyLow, keyHigh, max(r, g, b)).
  bool lumaKey = true;
  float keyLow = 0.02f;
  float keyHigh = 0.12f;
};

// Hooks the OpenXR loader. Must run before the game creates its OpenXR
// instance, so call it from setup() of an early mod.
void InstallXrHooks(const Settings &settings);

} // namespace passthrough
