#pragma once

namespace passthrough {

// Hooks the OpenXR loader. Must run before the game creates its OpenXR
// instance, so call it from setup() of an early mod.
void InstallXrHooks();

} // namespace passthrough
