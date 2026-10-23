#pragma once

namespace passthrough {

// Installs the scene hook. Call after il2cpp is initialised (late_load).
void InstallEnvironmentHooks();

// Re-applies the hide settings to all loaded scenes (shows or hides).
// Main thread only. Call after a settings change.
void ApplyEnvironment();

} // namespace passthrough
