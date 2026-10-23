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

// Returns the frame's layers, with every projection layer replaced by a copy
// that has XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT set, so the
// compositor blends the game over the layers below it using the game's alpha.
// The copies live in `storage`, which must outlive the returned pointers.
inline std::vector<const XrCompositionLayerBaseHeader *>
WithSourceAlpha(const XrCompositionLayerBaseHeader *const *layers, uint32_t count,
                std::vector<XrCompositionLayerProjection> &storage) {
  storage.clear();
  storage.reserve(count); // no reallocation, so pointers stay valid
  std::vector<const XrCompositionLayerBaseHeader *> out;
  out.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    if (layers[i]->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION) {
      out.push_back(layers[i]);
      continue;
    }
    auto &copy = storage.emplace_back(*reinterpret_cast<const XrCompositionLayerProjection *>(layers[i]));
    copy.layerFlags |= XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
    out.push_back(reinterpret_cast<const XrCompositionLayerBaseHeader *>(&copy));
  }
  return out;
}

} // namespace passthrough
