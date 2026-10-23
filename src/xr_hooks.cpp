// OpenXR interception for passthrough.
//
// Only one native function is inline-hooked: the OpenXR loader's exported
// xrGetInstanceProcAddr. When OVRPlugin looks up the functions below, we hand
// back our wrappers and keep the real pointers. The wrappers:
//   xrCreateInstance  - add XR_FB_passthrough to the enabled extensions
//   xrCreateSession   - create the passthrough feature + layer
//   xrEndFrame        - add the passthrough composition layer to every frame
//   xrDestroySession / xrDestroyInstance - clean up

#include "xr_hooks.hpp"

#include <dlfcn.h>

#include <atomic>
#include <cstring>
#include <mutex>

#include "main.hpp"
#include "xr_logic.hpp"

namespace {

using namespace passthrough;

// Phase 2: overlay at low opacity, to prove the layer works.
constexpr LayerPlacement kPlacement = LayerPlacement::Overlay;
constexpr float kOpacity = 0.3f;

PFN_xrGetInstanceProcAddr realGetInstanceProcAddr = nullptr;
PFN_xrCreateInstance realCreateInstance = nullptr;
PFN_xrDestroyInstance realDestroyInstance = nullptr;
PFN_xrCreateSession realCreateSession = nullptr;
PFN_xrDestroySession realDestroySession = nullptr;
PFN_xrEndFrame realEndFrame = nullptr;

PFN_xrCreatePassthroughFB createPassthrough = nullptr;
PFN_xrDestroyPassthroughFB destroyPassthrough = nullptr;
PFN_xrCreatePassthroughLayerFB createPassthroughLayer = nullptr;
PFN_xrDestroyPassthroughLayerFB destroyPassthroughLayer = nullptr;
PFN_xrPassthroughLayerSetStyleFB setPassthroughStyle = nullptr;

std::mutex stateMutex; // guards create/destroy of the handles below
XrInstance instance = XR_NULL_HANDLE;
std::atomic<XrSession> session{XR_NULL_HANDLE};
XrPassthroughFB passthroughHandle = XR_NULL_HANDLE;
std::atomic<XrPassthroughLayerFB> layerHandle{XR_NULL_HANDLE};
std::atomic<uint64_t> frameCount{0};

void logResult(const char *what, XrResult result) {
  if (XR_SUCCEEDED(result)) {
    PaperLogger.info("{} -> {}", what, static_cast<int>(result));
  } else {
    PaperLogger.error("{} -> {} (FAILED)", what, static_cast<int>(result));
  }
}

template <typename F> bool loadInstanceFunction(XrInstance inst, const char *name, F &out) {
  PFN_xrVoidFunction fn = nullptr;
  XrResult r = realGetInstanceProcAddr(inst, name, &fn);
  out = reinterpret_cast<F>(fn);
  if (XR_FAILED(r) || !fn) {
    logResult(name, r);
    return false;
  }
  return true;
}

void destroyPassthroughLocked() {
  if (auto layer = layerHandle.exchange(XR_NULL_HANDLE); layer != XR_NULL_HANDLE) {
    logResult("xrDestroyPassthroughLayerFB", destroyPassthroughLayer(layer));
  }
  if (passthroughHandle != XR_NULL_HANDLE) {
    logResult("xrDestroyPassthroughFB", destroyPassthrough(passthroughHandle));
    passthroughHandle = XR_NULL_HANDLE;
  }
}

void createPassthroughLocked(XrSession s) {
  if (!createPassthrough) {
    PaperLogger.warn("XR_FB_passthrough functions not loaded, no passthrough");
    return;
  }
  XrPassthroughCreateInfoFB ptInfo{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
  ptInfo.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
  XrResult r = createPassthrough(s, &ptInfo, &passthroughHandle);
  logResult("xrCreatePassthroughFB", r);
  if (XR_FAILED(r)) return;

  XrPassthroughLayerCreateInfoFB layerInfo{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
  layerInfo.passthrough = passthroughHandle;
  layerInfo.flags = XR_PASSTHROUGH_IS_RUNNING_AT_CREATION_BIT_FB;
  layerInfo.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
  XrPassthroughLayerFB layer = XR_NULL_HANDLE;
  r = createPassthroughLayer(s, &layerInfo, &layer);
  logResult("xrCreatePassthroughLayerFB", r);
  if (XR_FAILED(r)) {
    destroyPassthroughLocked();
    return;
  }

  XrPassthroughStyleFB style{XR_TYPE_PASSTHROUGH_STYLE_FB};
  style.textureOpacityFactor = kOpacity;
  style.edgeColor = {0, 0, 0, 0};
  logResult("xrPassthroughLayerSetStyleFB", setPassthroughStyle(layer, &style));

  layerHandle.store(layer);
}

// --- wrappers handed to OVRPlugin ---

XRAPI_ATTR XrResult XRAPI_CALL CreateInstance(const XrInstanceCreateInfo *info, XrInstance *out) {
  auto names = WithExtension(info->enabledExtensionNames, info->enabledExtensionCount,
                             XR_FB_PASSTHROUGH_EXTENSION_NAME);
  XrInstanceCreateInfo patched = *info;
  patched.enabledExtensionCount = static_cast<uint32_t>(names.size());
  patched.enabledExtensionNames = names.data();
  PaperLogger.info("xrCreateInstance: {} extensions requested, {} after adding {}",
                   info->enabledExtensionCount, names.size(), XR_FB_PASSTHROUGH_EXTENSION_NAME);

  XrResult r = realCreateInstance(&patched, out);
  logResult("xrCreateInstance (with passthrough)", r);
  if (XR_FAILED(r)) {
    // Do not break the game: try again exactly as the game asked.
    r = realCreateInstance(info, out);
    logResult("xrCreateInstance (original)", r);
    return r;
  }

  std::lock_guard lock(stateMutex);
  instance = *out;
  bool ok = loadInstanceFunction(instance, "xrCreatePassthroughFB", createPassthrough) &&
            loadInstanceFunction(instance, "xrDestroyPassthroughFB", destroyPassthrough) &&
            loadInstanceFunction(instance, "xrCreatePassthroughLayerFB", createPassthroughLayer) &&
            loadInstanceFunction(instance, "xrDestroyPassthroughLayerFB", destroyPassthroughLayer) &&
            loadInstanceFunction(instance, "xrPassthroughLayerSetStyleFB", setPassthroughStyle);
  if (!ok) createPassthrough = nullptr;
  PaperLogger.info("XR_FB_passthrough functions loaded: {}", ok);
  return r;
}

XRAPI_ATTR XrResult XRAPI_CALL DestroyInstance(XrInstance inst) {
  {
    std::lock_guard lock(stateMutex);
    if (inst == instance) {
      destroyPassthroughLocked();
      instance = XR_NULL_HANDLE;
      session.store(XR_NULL_HANDLE);
      createPassthrough = nullptr;
    }
  }
  return realDestroyInstance(inst);
}

XRAPI_ATTR XrResult XRAPI_CALL CreateSession(XrInstance inst, const XrSessionCreateInfo *info,
                                             XrSession *out) {
  XrResult r = realCreateSession(inst, info, out);
  logResult("xrCreateSession", r);
  if (XR_SUCCEEDED(r)) {
    std::lock_guard lock(stateMutex);
    destroyPassthroughLocked();
    session.store(*out);
    createPassthroughLocked(*out);
  }
  return r;
}

XRAPI_ATTR XrResult XRAPI_CALL DestroySession(XrSession s) {
  {
    std::lock_guard lock(stateMutex);
    if (s == session) {
      destroyPassthroughLocked();
      session.store(XR_NULL_HANDLE);
    }
  }
  return realDestroySession(s);
}

XRAPI_ATTR XrResult XRAPI_CALL EndFrame(XrSession s, const XrFrameEndInfo *info) {
  XrPassthroughLayerFB layer = layerHandle.load();
  if (layer == XR_NULL_HANDLE || s != session.load()) return realEndFrame(s, info);

  XrCompositionLayerPassthroughFB ptLayer{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
  ptLayer.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
  ptLayer.space = XR_NULL_HANDLE;
  ptLayer.layerHandle = layer;

  auto layers = WithPassthroughLayer(info->layers, info->layerCount,
                                     reinterpret_cast<const XrCompositionLayerBaseHeader *>(&ptLayer),
                                     kPlacement);
  XrFrameEndInfo patched = *info;
  patched.layerCount = static_cast<uint32_t>(layers.size());
  patched.layers = layers.data();
  XrResult r = realEndFrame(s, &patched);

  uint64_t n = frameCount.fetch_add(1);
  if (n == 0 || n % 900 == 0 || XR_FAILED(r)) {
    PaperLogger.info("xrEndFrame #{}: {} game layers + passthrough -> {}", n, info->layerCount,
                     static_cast<int>(r));
  }
  return r;
}

// Swap one of our wrappers in for `name`, keeping the real pointer.
template <typename F>
bool intercept(const char *name, const char *want, PFN_xrVoidFunction *fn, F &real, F wrapper) {
  if (std::strcmp(name, want) != 0) return false;
  real = reinterpret_cast<F>(*fn);
  *fn = reinterpret_cast<PFN_xrVoidFunction>(wrapper);
  PaperLogger.info("intercepted {}", name);
  return true;
}

} // namespace

MAKE_HOOK_NO_CATCH(xrGetInstanceProcAddr_hook, nullptr, XrResult, XrInstance inst, const char *name,
                   PFN_xrVoidFunction *fn) {
  XrResult r = xrGetInstanceProcAddr_hook(inst, name, fn);
  if (XR_FAILED(r) || !fn || !*fn || !name) return r;
  intercept(name, "xrCreateInstance", fn, realCreateInstance, &CreateInstance) ||
      intercept(name, "xrDestroyInstance", fn, realDestroyInstance, &DestroyInstance) ||
      intercept(name, "xrCreateSession", fn, realCreateSession, &CreateSession) ||
      intercept(name, "xrDestroySession", fn, realDestroySession, &DestroySession) ||
      intercept(name, "xrEndFrame", fn, realEndFrame, &EndFrame);
  return r;
}

namespace passthrough {

void InstallXrHooks() {
  // The game's OpenXR loader lives in the APK lib dir. Loading it here (if
  // libunity has not yet) gives the same instance OVRPlugin uses later.
  void *loader = dlopen("libopenxr_loader.so", RTLD_NOW);
  if (!loader) {
    PaperLogger.error("dlopen libopenxr_loader.so failed: {}", dlerror());
    return;
  }
  void *gipa = dlsym(loader, "xrGetInstanceProcAddr");
  if (!gipa) {
    PaperLogger.error("dlsym xrGetInstanceProcAddr failed: {}", dlerror());
    return;
  }
  INSTALL_HOOK_DIRECT(PaperLogger, xrGetInstanceProcAddr_hook, gipa);
  realGetInstanceProcAddr = Hook_xrGetInstanceProcAddr_hook::xrGetInstanceProcAddr_hook;
  PaperLogger.info("hooked xrGetInstanceProcAddr at {}", gipa);
}

} // namespace passthrough
