#pragma once

namespace passthrough {

// Unity layers that hold Beat Saber's song environment (found with the render
// dump on 1.40.8): 13 = environment lights (BoxLight, BakedBloom),
// 14 = environment geometry (rings, ...), 29 = BloomSkyboxQuad (the glowing
// background). Notes (8), debris (9) and sabers (12) are on other layers.
constexpr int kEnvironmentLayers = (1 << 13) | (1 << 14) | (1 << 29);

// The camera culling mask with the environment layers removed (hide) or as
// the game set it (show). `original` is the mask the game chose.
constexpr int EnvironmentCullingMask(int original, bool hide) {
  return hide ? (original & ~kEnvironmentLayers) : original;
}

} // namespace passthrough
