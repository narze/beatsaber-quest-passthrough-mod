#pragma once

#include "settings.hpp"

namespace passthrough {

// Hooks the OpenXR loader. Must run before the game creates its OpenXR
// instance, so call it from setup() of an early mod. Reads CurrentSettings()
// on every frame, so setting changes apply live.
void InstallXrHooks();

} // namespace passthrough
