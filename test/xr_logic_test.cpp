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

static void source_alpha_only_on_projection_layers() {
  XrCompositionLayerProjection proj{};
  proj.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
  proj.layerFlags = XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
  XrCompositionLayerQuad quad{};
  quad.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
  const XrCompositionLayerBaseHeader *in[] = {
      reinterpret_cast<const XrCompositionLayerBaseHeader *>(&proj),
      reinterpret_cast<const XrCompositionLayerBaseHeader *>(&quad)};
  std::vector<XrCompositionLayerProjection> storage;
  auto out = WithSourceAlpha(in, 2, storage);
  assert(out.size() == 2);
  // Projection layer is a modified copy; the game's struct stays unchanged.
  assert(out[0] != in[0]);
  auto *copy = reinterpret_cast<const XrCompositionLayerProjection *>(out[0]);
  assert(copy->layerFlags == (XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT |
                              XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT));
  assert(proj.layerFlags == XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT);
  // Other layers pass through as they are.
  assert(out[1] == in[1]);
}

static void source_alpha_storage_survives_many_layers() {
  XrCompositionLayerProjection a{};
  a.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
  XrCompositionLayerProjection b{};
  b.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
  const XrCompositionLayerBaseHeader *in[] = {
      reinterpret_cast<const XrCompositionLayerBaseHeader *>(&a),
      reinterpret_cast<const XrCompositionLayerBaseHeader *>(&b)};
  std::vector<XrCompositionLayerProjection> storage;
  auto out = WithSourceAlpha(in, 2, storage);
  // Pointers must stay valid (no reallocation after taking them).
  assert(out[0] == reinterpret_cast<const XrCompositionLayerBaseHeader *>(&storage[0]));
  assert(out[1] == reinterpret_cast<const XrCompositionLayerBaseHeader *>(&storage[1]));
}

int main() {
  source_alpha_only_on_projection_layers();
  source_alpha_storage_survives_many_layers();
  adds_missing_extension();
  keeps_existing_extension_once();
  handles_empty_extension_list();
  overlay_goes_last();
  underlay_goes_first();
  empty_frame_still_gets_layer();
  respects_max_layer_count();
  std::puts("all xr_logic tests passed");
}
