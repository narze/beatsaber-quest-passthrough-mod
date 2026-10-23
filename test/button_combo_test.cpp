// Host unit tests for ButtonCombo. Run: ./test/run.sh
#include "button_combo.hpp"

#include <cassert>
#include <cstdio>

using namespace passthrough;

static void fires_once_when_both_are_down() {
  ButtonCombo c;
  assert(!c.Update(true, false));
  assert(c.Update(true, true));  // both down: fire
  assert(!c.Update(true, true)); // still held: no repeat
  assert(!c.Update(true, false));
  assert(c.Update(true, true)); // pressed again: fire again
}

static void needs_both_buttons() {
  ButtonCombo c;
  assert(!c.Update(false, true));
  assert(!c.Update(true, false));
  assert(!c.Update(false, false));
}

static void starts_released() {
  ButtonCombo c;
  assert(c.Update(true, true)); // held at the first check: fire
}

int main() {
  fires_once_when_both_are_down();
  needs_both_buttons();
  starts_released();
  std::puts("all button_combo tests passed");
}
