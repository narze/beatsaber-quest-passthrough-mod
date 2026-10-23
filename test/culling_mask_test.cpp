// Host unit tests for the environment culling mask. Run: ./test/run.sh
#include "culling_mask.hpp"

#include <cassert>
#include <cstdio>

using namespace passthrough;

constexpr int bit(int layer) { return 1 << layer; }

static void hide_removes_environment_layers() {
  int original = 0x3819fd37; // gameplay MainCamera mask seen on the headset
  int m = EnvironmentCullingMask(original, true);
  assert((m & bit(13)) == 0);
  assert((m & bit(14)) == 0);
  assert((m & bit(29)) == 0);
  // Notes (8) and sabers (12) stay. (Debris, 9, is off in this mask.)
  assert(m & bit(8));
  assert(m & bit(12));
  // Everything that is not an environment layer is unchanged.
  assert((m | kEnvironmentLayers) == (original | kEnvironmentLayers));
}

static void show_restores_original() {
  int original = 0x3819fd37;
  assert(EnvironmentCullingMask(original, false) == original);
}

static void never_adds_layers_the_game_did_not_use() {
  int original = bit(8) | bit(12); // no environment layers at all
  assert(EnvironmentCullingMask(original, false) == original);
  assert(EnvironmentCullingMask(original, true) == original);
}

int main() {
  hide_removes_environment_layers();
  show_restores_original();
  never_adds_layers_the_game_did_not_use();
  std::puts("all culling_mask tests passed");
}
