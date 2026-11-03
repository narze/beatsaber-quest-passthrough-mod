// Host unit tests for ButtonCombo. Run: ./test/run.sh
#include "button_combo.hpp"

#include <cassert>
#include <cstdio>

using namespace passthrough;
using namespace passthrough::Keybind;

static void fires_once_when_the_combo_is_down() {
  ButtonCombo c;
  uint32_t ab = kA | kB;
  assert(!c.Update(ab, kA));
  assert(c.Update(ab, kA | kB));  // all down: fire
  assert(!c.Update(ab, kA | kB)); // still held: no repeat
  assert(!c.Update(ab, kA));
  assert(c.Update(ab, kA | kB)); // pressed again: fire again
}

static void needs_every_button() {
  ButtonCombo c;
  uint32_t ab = kA | kB;
  assert(!c.Update(ab, kB));
  assert(!c.Update(ab, kA));
  assert(!c.Update(ab, 0));
}

static void single_button_combo() {
  ButtonCombo c;
  assert(c.Update(kLeftStick, kLeftStick));
  assert(!c.Update(kLeftStick, kLeftStick));
  assert(!c.Update(kLeftStick, 0));
  assert(c.Update(kLeftStick, kLeftStick | kA)); // extra buttons are fine
}

static void off_never_fires() {
  ButtonCombo c;
  assert(!c.Update(0, 0));
  assert(!c.Update(0, kA | kB | kX | kY | kLeftStick | kRightStick));
}

static void starts_released() {
  ButtonCombo c;
  assert(c.Update(kA | kB, kA | kB)); // held at the first check: fire
}

int main() {
  fires_once_when_the_combo_is_down();
  needs_every_button();
  single_button_combo();
  off_never_fires();
  starts_released();
  std::puts("all button_combo tests passed");
}
