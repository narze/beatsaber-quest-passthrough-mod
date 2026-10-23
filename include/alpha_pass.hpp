#pragma once

#include <cstdint>

#include "swapchain_registry.hpp"

namespace passthrough {

// A full-screen GL pass over an eye-buffer texture that replaces its alpha
// with a key of the pixel's brightness: alpha = smoothstep(low, high,
// max(r, g, b)). Black (the hidden environment) becomes alpha 0, so the
// passthrough layer below shows; lit objects become opaque.
//
// Beat Saber writes glow strength into alpha, so its own alpha is not usable
// as coverage. Must run on the game's render thread (GL context current).
// Saves and restores all GL state it touches, so Unity's state cache stays
// valid.
class AlphaPass {
public:
  // Returns false if the pass could not run (the frame is left unchanged).
  // showMask: debug, write the key as an opaque grayscale image.
  bool Apply(const ReleaseTargetInfo &target, float low, float high, bool showMask = false);

private:
  bool Init();
  bool initDone_ = false;
  bool failed_ = false;
  uint32_t program_ = 0;
  uint32_t vao_ = 0;
  uint32_t fbo_ = 0;
  int32_t keyLocation_ = -1;
  int32_t showMaskLocation_ = -1;
  uint64_t applied_ = 0;
};

} // namespace passthrough
