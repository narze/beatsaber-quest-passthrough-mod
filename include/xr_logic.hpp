#pragma once

// Pure list logic for the OpenXR hooks. No Android or game dependencies, so
// it is unit tested on the host (test/xr_logic_test.cpp).

#include <cstdint>
#include <cstring>
#include <vector>

#include "openxr/openxr.h"

namespace passthrough {

// Returns the extension names plus `ext`, if it is not already in the list.
inline std::vector<const char *> WithExtension(const char *const *names, uint32_t count,
                                               const char *ext) {
  std::vector<const char *> out(names, names + count);
  for (auto *name : out) {
    if (std::strcmp(name, ext) == 0) return out;
  }
  out.push_back(ext);
  return out;
}

enum class LayerPlacement {
  Overlay,  // composited on top of the game (Phase 2 test)
  Underlay, // composited below the game (background)
};

// Returns the frame's layers with `passthrough` added. When the frame already
// has `maxLayers` layers, returns them unchanged.
inline std::vector<const XrCompositionLayerBaseHeader *>
WithPassthroughLayer(const XrCompositionLayerBaseHeader *const *layers, uint32_t count,
                     const XrCompositionLayerBaseHeader *passthrough, LayerPlacement placement,
                     uint32_t maxLayers = 16) {
  std::vector<const XrCompositionLayerBaseHeader *> out(layers, layers + count);
  if (out.size() >= maxLayers) return out;
  if (placement == LayerPlacement::Underlay) {
    out.insert(out.begin(), passthrough);
  } else {
    out.push_back(passthrough);
  }
  return out;
}

} // namespace passthrough
