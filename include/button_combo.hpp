#pragma once

// Detects a combo press. Update() returns true once when every button of the
// combo becomes down; the combo must be released before it fires again.
// No game dependencies, so it is unit tested on the host.

#include <cstdint>

#include "keybind.hpp"

namespace passthrough {

class ButtonCombo {
public:
  // combo, pressed: Keybind::Button bits.
  bool Update(uint32_t combo, uint32_t pressed) {
    bool down = Keybind::IsHeld(combo, pressed);
    bool fire = down && !wasDown_;
    wasDown_ = down;
    return fire;
  }

private:
  bool wasDown_ = false;
};

} // namespace passthrough
