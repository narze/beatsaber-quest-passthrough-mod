// Host unit tests for the pure OpenXR list logic. Run: ./test/run.sh
#include "xr_logic.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace passthrough;

static void adds_missing_extension() {
  const char *in[] = {"XR_KHR_android_create_instance", "XR_KHR_opengl_es_enable"};
  auto out = WithExtension(in, 2, "XR_FB_passthrough");
  assert(out.size() == 3);
  assert(std::strcmp(out[0], in[0]) == 0);
  assert(std::strcmp(out[1], in[1]) == 0);
  assert(std::strcmp(out[2], "XR_FB_passthrough") == 0);
}

static void keeps_existing_extension_once() {
  const char *in[] = {"XR_FB_passthrough", "XR_KHR_opengl_es_enable"};
  auto out = WithExtension(in, 2, "XR_FB_passthrough");
  assert(out.size() == 2);
}

static void handles_empty_extension_list() {
  auto out = WithExtension(nullptr, 0, "XR_FB_passthrough");
  assert(out.size() == 1);
  assert(std::strcmp(out[0], "XR_FB_passthrough") == 0);
}

static XrCompositionLayerBaseHeader layer(XrStructureType type) {
  return XrCompositionLayerBaseHeader{type, nullptr, 0, XR_NULL_HANDLE};
}

static void overlay_goes_last() {
  auto proj = layer(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
  auto quad = layer(XR_TYPE_COMPOSITION_LAYER_QUAD);
  auto pt = layer(XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB);
  const XrCompositionLayerBaseHeader *in[] = {&proj, &quad};
  auto out = WithPassthroughLayer(in, 2, &pt, LayerPlacement::Overlay);
  assert(out.size() == 3);
  assert(out[0] == &proj && out[1] == &quad && out[2] == &pt);
}

static void underlay_goes_first() {
  auto proj = layer(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
  auto pt = layer(XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB);
  const XrCompositionLayerBaseHeader *in[] = {&proj};
  auto out = WithPassthroughLayer(in, 1, &pt, LayerPlacement::Underlay);
  assert(out.size() == 2);
  assert(out[0] == &pt && out[1] == &proj);
}

static void empty_frame_still_gets_layer() {
  auto pt = layer(XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB);
  auto out = WithPassthroughLayer(nullptr, 0, &pt, LayerPlacement::Overlay);
  assert(out.size() == 1 && out[0] == &pt);
}

static void respects_max_layer_count() {
  auto proj = layer(XR_TYPE_COMPOSITION_LAYER_PROJECTION);
  auto pt = layer(XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB);
  const XrCompositionLayerBaseHeader *in[] = {&proj, &proj};
  // No room for one more layer: send the frame unchanged.
  auto out = WithPassthroughLayer(in, 2, &pt, LayerPlacement::Overlay, 2);
  assert(out.size() == 2 && out[0] == &proj && out[1] == &proj);
}

int main() {
  adds_missing_extension();
  keeps_existing_extension_once();
  handles_empty_extension_list();
  overlay_goes_last();
  underlay_goes_first();
  empty_frame_still_gets_layer();
  respects_max_layer_count();
  std::puts("all xr_logic tests passed");
}
