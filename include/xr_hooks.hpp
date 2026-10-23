#pragma once

#include "xr_logic.hpp"

namespace passthrough {

struct Settings {
  LayerPlacement placement = LayerPlacement::Underlay;
  float opacity = 1.0f;    // passthrough texture opacity
  bool sourceAlpha = true; // blend the game over passthrough with its alpha
};

// Hooks the OpenXR loader. Must run before the game creates its OpenXR
// instance, so call it from setup() of an early mod.
void InstallXrHooks(const Settings &settings);

} // namespace passthrough
