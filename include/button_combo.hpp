#pragma once

// Detects a two-button combo press. Update() returns true once when both
// buttons become down; the buttons must be released before it fires again.
// No game dependencies, so it is unit tested on the host.

namespace passthrough {

class ButtonCombo {
public:
  bool Update(bool first, bool second) {
    bool down = first && second;
    bool fire = down && !wasDown_;
    wasDown_ = down;
    return fire;
  }

private:
  bool wasDown_ = false;
};

} // namespace passthrough
