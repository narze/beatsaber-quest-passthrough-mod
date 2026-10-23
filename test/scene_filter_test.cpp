// Host unit tests for the environment scene filter. Run: ./test/run.sh
#include "scene_filter.hpp"

#include <cassert>
#include <cstdio>

using passthrough::IsEnvironmentScene;
using passthrough::ShouldHideScene;

static void gameplay_environments_match() {
  assert(IsEnvironmentScene("DefaultEnvironment", false));
  assert(IsEnvironmentScene("GridEnvironment", false));
  assert(IsEnvironmentScene("BillieEnvironment", false));
}

static void core_scenes_never_match() {
  assert(!IsEnvironmentScene("GameCore", true));
  assert(!IsEnvironmentScene("StandardGameplay", true));
  assert(!IsEnvironmentScene("MainMenu", true));
  assert(!IsEnvironmentScene("MenuCore", true));
  assert(!IsEnvironmentScene("", true));
  assert(!IsEnvironmentScene("Environment", true)); // no prefix: not a real env scene
}

static void menu_environment_only_when_enabled() {
  assert(IsEnvironmentScene("MainMenuEnvironment", true));
  assert(!IsEnvironmentScene("MainMenuEnvironment", false));
  assert(IsEnvironmentScene("MenuEnvironment", true));
  assert(!IsEnvironmentScene("MenuEnvironment", false));
}

static void song_and_menu_toggles_are_independent() {
  // hideSong, hideMenu
  assert(ShouldHideScene("DefaultEnvironment", true, false));
  assert(!ShouldHideScene("DefaultEnvironment", false, true));
  assert(ShouldHideScene("MainMenuEnvironment", false, true)); // the reported bug
  assert(!ShouldHideScene("MainMenuEnvironment", true, false));
  assert(!ShouldHideScene("GameCore", true, true));
}

int main() {
  song_and_menu_toggles_are_independent();
  gameplay_environments_match();
  core_scenes_never_match();
  menu_environment_only_when_enabled();
  std::puts("all scene_filter tests passed");
}
