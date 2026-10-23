// Host unit tests for SwapchainRegistry. Run: ./test/run.sh
#include "swapchain_registry.hpp"

#include <cassert>
#include <cstdio>

using passthrough::SwapchainRegistry;

static void unknown_swapchain_has_no_target() {
  SwapchainRegistry r;
  assert(!r.ReleaseTarget(1).has_value());
}

static void projection_swapchain_returns_acquired_texture() {
  SwapchainRegistry r;
  r.OnCreate(7, {2016, 2112, 2, 0x8C43});
  r.OnImages(7, {101, 102, 103});
  r.OnAcquire(7, 2);
  r.MarkProjection(7);
  auto t = r.ReleaseTarget(7);
  assert(t.has_value());
  assert(t->texture == 103);
  assert(t->info.width == 2016 && t->info.height == 2112 && t->info.arraySize == 2);
  assert(t->info.format == 0x8C43);
}

static void non_projection_swapchain_is_ignored() {
  SwapchainRegistry r;
  r.OnCreate(8, {1, 1, 1, 0});
  r.OnImages(8, {201});
  r.OnAcquire(8, 0);
  assert(!r.ReleaseTarget(8).has_value());
}

static void no_target_before_images_or_acquire() {
  SwapchainRegistry r;
  r.OnCreate(7, {2016, 2112, 2, 0});
  r.MarkProjection(7);
  assert(!r.ReleaseTarget(7).has_value()); // no images yet
  r.OnImages(7, {101});
  assert(!r.ReleaseTarget(7).has_value()); // nothing acquired yet
  r.OnAcquire(7, 5);                         // index out of range
  assert(!r.ReleaseTarget(7).has_value());
}

static void destroy_forgets_swapchain() {
  SwapchainRegistry r;
  r.OnCreate(7, {2016, 2112, 2, 0});
  r.OnImages(7, {101});
  r.OnAcquire(7, 0);
  r.MarkProjection(7);
  r.OnDestroy(7);
  assert(!r.ReleaseTarget(7).has_value());
}

int main() {
  unknown_swapchain_has_no_target();
  projection_swapchain_returns_acquired_texture();
  non_projection_swapchain_is_ignored();
  no_target_before_images_or_acquire();
  destroy_forgets_swapchain();
  std::puts("all swapchain_registry tests passed");
}
