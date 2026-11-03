#pragma once

// Controller combo presets for the passthrough quick toggle. The same presets
// as the reset keybind of the browser mod (beatsaber-quest-browser-mod), with
// A + B as the default. No game dependencies, so it is unit tested on the host.

#include <cstdint>
#include <span>
#include <string_view>

namespace passthrough::Keybind {

// One bit per controller button.
enum Button : uint32_t {
  kX = 1u << 0,
  kY = 1u << 1,
  kA = 1u << 2,
  kB = 1u << 3,
  kLeftStick = 1u << 4,  // left thumbstick click
  kRightStick = 1u << 5, // right thumbstick click
};

struct Preset {
  std::string_view name; // shown in the settings and saved in the config
  uint32_t buttons;      // 0 = off
};

constexpr uint32_t kDefaultButtons = kA | kB;

inline std::span<const Preset> Presets() {
  static constexpr Preset presets[] = {
      {"Off", 0},
      {"A + B", kA | kB},
      {"X + Y", kX | kY},
      {"X + A", kX | kA},
      {"Y + B", kY | kB},
      {"Left stick click", kLeftStick},
      {"Right stick click", kRightStick},
      {"Both stick clicks", kLeftStick | kRightStick},
  };
  return presets;
}

inline bool IsPreset(uint32_t buttons) {
  for (const auto &p : Presets())
    if (p.buttons == buttons) return true;
  return false;
}

// The preset with this name, else the default.
inline Preset Find(std::string_view name) {
  for (const auto &p : Presets())
    if (p.name == name) return p;
  for (const auto &p : Presets())
    if (p.buttons == kDefaultButtons) return p;
  return {};
}

// The name of the preset with these buttons, else the default's name.
inline std::string_view NameOf(uint32_t buttons) {
  for (const auto &p : Presets())
    if (p.buttons == buttons) return p.name;
  return NameOf(kDefaultButtons);
}

// True when every button of the combo is pressed. Extra buttons do not
// matter. Never true for Off (0).
inline bool IsHeld(uint32_t combo, uint32_t pressed) { return combo != 0 && (pressed & combo) == combo; }

} // namespace passthrough::Keybind
