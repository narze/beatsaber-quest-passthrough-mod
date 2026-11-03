// Host unit tests for the controller combo presets. Run: ./test/run.sh
#include "keybind.hpp"

#include <cassert>
#include <cstdio>
#include <set>
#include <string_view>

using namespace passthrough::Keybind;

static void presets_include_off_and_common_combos() {
  assert(Find("Off").buttons == 0);
  assert(Find("A + B").buttons == (kA | kB));
  assert(Find("X + Y").buttons == (kX | kY));
  assert(Find("Left stick click").buttons == kLeftStick);
  assert(Find("Both stick clicks").buttons == (kLeftStick | kRightStick));
}

static void default_is_a_plus_b() {
  assert(kDefaultButtons == (kA | kB));
  assert(Find("no such combo").buttons == kDefaultButtons); // unknown saved name
}

static void names_and_buttons_are_unique() {
  std::set<std::string_view> names;
  std::set<uint32_t> buttons;
  for (const auto &p : Presets()) {
    assert(names.insert(p.name).second);
    assert(buttons.insert(p.buttons).second);
    assert(Find(p.name).buttons == p.buttons);
    assert(NameOf(p.buttons) == p.name);
    assert(IsPreset(p.buttons));
  }
}

static void unknown_buttons_are_not_a_preset() {
  assert(!IsPreset(kA | kX | kLeftStick));
  assert(NameOf(kA | kX | kLeftStick) == NameOf(kDefaultButtons));
}

static void is_held_needs_every_button() {
  uint32_t ab = kA | kB;
  assert(!IsHeld(ab, kA));
  assert(!IsHeld(ab, kB));
  assert(IsHeld(ab, kA | kB));
  assert(IsHeld(ab, kA | kB | kX)); // extra buttons are fine
  assert(IsHeld(kRightStick, kRightStick));
}

static void off_is_never_held() {
  assert(!IsHeld(0, kX | kY | kA | kB | kLeftStick | kRightStick));
}

int main() {
  presets_include_off_and_common_combos();
  default_is_a_plus_b();
  names_and_buttons_are_unique();
  unknown_buttons_are_not_a_preset();
  is_held_needs_every_button();
  off_is_never_held();
  std::puts("all keybind tests passed");
}
