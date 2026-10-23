#pragma once

// All mod settings, shared by the OpenXR hooks (render thread), the scene
// hook and the settings UI (main thread). No game dependencies, so it is unit
// tested on the host.

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <mutex>

#include "xr_logic.hpp"

namespace passthrough {

struct Settings {
  bool enabled = true; // master switch: off = the game exactly as without the mod
  LayerPlacement placement = LayerPlacement::Underlay;
  float opacity = 1.0f;    // passthrough texture opacity
  bool sourceAlpha = true; // blend the game over passthrough with its alpha
  // Replace the game's alpha (glow strength in Beat Saber) with a key of the
  // pixel brightness: alpha = smoothstep(keyLow, keyHigh, max(r, g, b)).
  bool lumaKey = true;
  float keyLow = 0.0f;
  float keyHigh = 0.015f;
  bool hideEnvironment = true;     // gameplay environments
  bool hideMenuEnvironment = true; // the main menu environment
  // Debug: draw the luma key as grayscale (white = game, black = room).
  // Not saved.
  bool showMask = false;
};

// Clamps values to valid ranges and keeps keyLow < keyHigh.
inline Settings Sanitize(Settings s) {
  s.opacity = std::clamp(s.opacity, 0.0f, 1.0f);
  s.keyLow = std::clamp(s.keyLow, 0.0f, 1.0f);
  s.keyHigh = std::clamp(s.keyHigh, 0.0f, 1.0f);
  constexpr float minGap = 0.001f;
  if (s.keyHigh < s.keyLow + minGap) {
    if (s.keyLow + minGap <= 1.0f) {
      s.keyHigh = s.keyLow + minGap;
    } else {
      s.keyLow = 1.0f - minGap;
      s.keyHigh = 1.0f;
    }
  }
  return s;
}

// Thread-safe holder. Get() is cheap enough to call once per frame.
class LiveSettings {
public:
  Settings Get() const {
    std::lock_guard lock(mutex_);
    return value_;
  }

  void Set(const Settings &s) {
    std::lock_guard lock(mutex_);
    value_ = Sanitize(s);
    version_.fetch_add(1);
  }

  template <typename F> void Update(F &&change) {
    std::lock_guard lock(mutex_);
    Settings s = value_;
    change(s);
    value_ = Sanitize(s);
    version_.fetch_add(1);
  }

  uint64_t Version() const { return version_.load(); }

private:
  mutable std::mutex mutex_;
  Settings value_;
  std::atomic<uint64_t> version_{0};
};

// The one instance used by the mod.
inline LiveSettings &CurrentSettings() {
  static LiveSettings instance;
  return instance;
}

} // namespace passthrough
