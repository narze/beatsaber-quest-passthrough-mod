// OpenXR interception for passthrough.
//
// Only one native function is inline-hooked: the OpenXR loader's exported
// xrGetInstanceProcAddr. When OVRPlugin looks up the functions below, we hand
// back our wrappers and keep the real pointers. The wrappers:
//   xrCreateInstance  - add XR_FB_passthrough to the enabled extensions
//   xrCreateSession   - create the passthrough feature + layer
//   xrEndFrame        - add the passthrough composition layer to every frame
//   xrDestroySession / xrDestroyInstance - clean up
//   xrCreateSwapchain / xrEnumerateSwapchainImages / xrAcquireSwapchainImage
//                     - track the GL texture of each eye-buffer image
//   xrReleaseSwapchainImage - run the brightness alpha key on the eye buffer
//                     (see alpha_pass.hpp) before the compositor gets it

#include "xr_hooks.hpp"

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <dlfcn.h>

#include <atomic>
#include <cstring>
#include <mutex>

#define XR_USE_GRAPHICS_API_OPENGL_ES
#include "openxr/openxr_platform.h"

#include "alpha_pass.hpp"
#include "main.hpp"
#include "swapchain_registry.hpp"
#include "xr_logic.hpp"

namespace {

using namespace passthrough;

Settings settings; // set once in setup(), before any OpenXR call

PFN_xrGetInstanceProcAddr realGetInstanceProcAddr = nullptr;
PFN_xrCreateInstance realCreateInstance = nullptr;
PFN_xrDestroyInstance realDestroyInstance = nullptr;
PFN_xrCreateSession realCreateSession = nullptr;
PFN_xrDestroySession realDestroySession = nullptr;
PFN_xrEndFrame realEndFrame = nullptr;
PFN_xrCreateSwapchain realCreateSwapchain = nullptr;
PFN_xrDestroySwapchain realDestroySwapchain = nullptr;
PFN_xrEnumerateSwapchainImages realEnumerateSwapchainImages = nullptr;
PFN_xrAcquireSwapchainImage realAcquireSwapchainImage = nullptr;
PFN_xrReleaseSwapchainImage realReleaseSwapchainImage = nullptr;

SwapchainRegistry swapchains;
AlphaPass alphaPass; // render thread only

uint64_t key(XrSwapchain s) { return reinterpret_cast<uint64_t>(s); }

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
  style.textureOpacityFactor = settings.opacity;
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

  // Underlay: the game must be blended over the passthrough with its alpha.
  thread_local std::vector<XrCompositionLayerProjection> projStorage;
  auto gameLayers = settings.sourceAlpha
                        ? WithSourceAlpha(info->layers, info->layerCount, projStorage)
                        : std::vector<const XrCompositionLayerBaseHeader *>(
                              info->layers, info->layers + info->layerCount);
  auto layers = WithPassthroughLayer(gameLayers.data(), static_cast<uint32_t>(gameLayers.size()),
                                     reinterpret_cast<const XrCompositionLayerBaseHeader *>(&ptLayer),
                                     settings.placement);
  if (settings.lumaKey) {
    for (uint32_t i = 0; i < info->layerCount; i++) {
      if (info->layers[i]->type != XR_TYPE_COMPOSITION_LAYER_PROJECTION) continue;
      auto *proj = reinterpret_cast<const XrCompositionLayerProjection *>(info->layers[i]);
      for (uint32_t v = 0; v < proj->viewCount; v++) {
        swapchains.MarkProjection(key(proj->views[v].subImage.swapchain));
      }
    }
  }

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

XRAPI_ATTR XrResult XRAPI_CALL CreateSwapchain(XrSession s, const XrSwapchainCreateInfo *info,
                                               XrSwapchain *out) {
  XrResult r = realCreateSwapchain(s, info, out);
  if (XR_SUCCEEDED(r)) {
    swapchains.OnCreate(key(*out), {info->width, info->height, info->arraySize, info->format});
    PaperLogger.info("xrCreateSwapchain {}x{}x{} format 0x{:x} usage 0x{:x} samples {}", info->width,
                     info->height, info->arraySize, info->format, info->usageFlags, info->sampleCount);
  }
  return r;
}

XRAPI_ATTR XrResult XRAPI_CALL DestroySwapchain(XrSwapchain sc) {
  swapchains.OnDestroy(key(sc));
  return realDestroySwapchain(sc);
}

XRAPI_ATTR XrResult XRAPI_CALL EnumerateSwapchainImages(XrSwapchain sc, uint32_t capacity,
                                                        uint32_t *count,
                                                        XrSwapchainImageBaseHeader *images) {
  XrResult r = realEnumerateSwapchainImages(sc, capacity, count, images);
  if (XR_SUCCEEDED(r) && images && capacity > 0 &&
      images->type == XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR) {
    auto *gl = reinterpret_cast<XrSwapchainImageOpenGLESKHR *>(images);
    std::vector<uint32_t> textures;
    for (uint32_t i = 0; i < *count; i++) textures.push_back(gl[i].image);
    swapchains.OnImages(key(sc), std::move(textures));
  }
  return r;
}

XRAPI_ATTR XrResult XRAPI_CALL AcquireSwapchainImage(XrSwapchain sc,
                                                     const XrSwapchainImageAcquireInfo *info,
                                                     uint32_t *index) {
  XrResult r = realAcquireSwapchainImage(sc, info, index);
  if (XR_SUCCEEDED(r)) swapchains.OnAcquire(key(sc), *index);
  return r;
}

XRAPI_ATTR XrResult XRAPI_CALL ReleaseSwapchainImage(XrSwapchain sc,
                                                     const XrSwapchainImageReleaseInfo *info) {
  if (settings.lumaKey && layerHandle.load() != XR_NULL_HANDLE) {
    if (auto target = swapchains.ReleaseTarget(key(sc))) {
      alphaPass.Apply(*target, settings.keyLow, settings.keyHigh);
    }
  }
  return realReleaseSwapchainImage(sc, info);
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
      intercept(name, "xrEndFrame", fn, realEndFrame, &EndFrame) ||
      intercept(name, "xrCreateSwapchain", fn, realCreateSwapchain, &CreateSwapchain) ||
      intercept(name, "xrDestroySwapchain", fn, realDestroySwapchain, &DestroySwapchain) ||
      intercept(name, "xrEnumerateSwapchainImages", fn, realEnumerateSwapchainImages,
                &EnumerateSwapchainImages) ||
      intercept(name, "xrAcquireSwapchainImage", fn, realAcquireSwapchainImage,
                &AcquireSwapchainImage) ||
      intercept(name, "xrReleaseSwapchainImage", fn, realReleaseSwapchainImage,
                &ReleaseSwapchainImage);
  return r;
}

namespace passthrough {

void InstallXrHooks(const Settings &s) {
  settings = s;
  PaperLogger.info("settings: placement={} opacity={} sourceAlpha={} lumaKey={} key={}..{}",
                   settings.placement == LayerPlacement::Underlay ? "underlay" : "overlay",
                   settings.opacity, settings.sourceAlpha, settings.lumaKey, settings.keyLow,
                   settings.keyHigh);
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
