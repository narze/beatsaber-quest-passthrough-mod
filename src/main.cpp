#include "main.hpp"

#include <string>

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "scotland2/shared/modloader.h"
#include "xr_hooks.hpp"

// Stores the ID and version of our mod, and is sent to
// the modloader upon startup
static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

// Reads Configs/Passthrough.json and writes back any missing keys with their
// defaults, so the file always shows every setting:
//   placement: "underlay" (background) or "overlay" (on top, for testing)
//   opacity:   0..1, passthrough opacity
//   sourceAlpha: blend the game over passthrough with the game's alpha
static passthrough::Settings loadSettings() {
  passthrough::Settings s;
  Configuration config(modInfo);
  config.Load();
  auto &doc = config.config;
  if (!doc.IsObject()) doc.SetObject();
  auto &alloc = doc.GetAllocator();
  bool changed = false;

  if (!doc.HasMember("placement") || !doc["placement"].IsString()) {
    doc.RemoveMember("placement");
    doc.AddMember("placement", "underlay", alloc);
    changed = true;
  }
  std::string placement = doc["placement"].GetString();
  s.placement = placement == "overlay" ? passthrough::LayerPlacement::Overlay
                                       : passthrough::LayerPlacement::Underlay;

  if (!doc.HasMember("opacity") || !doc["opacity"].IsNumber()) {
    doc.RemoveMember("opacity");
    doc.AddMember("opacity", s.opacity, alloc);
    changed = true;
  }
  s.opacity = doc["opacity"].GetFloat();

  if (!doc.HasMember("sourceAlpha") || !doc["sourceAlpha"].IsBool()) {
    doc.RemoveMember("sourceAlpha");
    doc.AddMember("sourceAlpha", s.sourceAlpha, alloc);
    changed = true;
  }
  s.sourceAlpha = doc["sourceAlpha"].GetBool();

  if (changed) config.Write();
  return s;
}

// Called at the early stages of game loading
MOD_EXTERN_FUNC void setup(CModInfo *info) noexcept {
  *info = modInfo.to_c();

  // File logging
  Paper::Logger::RegisterFileContextId(PaperLogger.tag);

  PaperLogger.info("Passthrough {} setup()", VERSION);

  // Must be before the game creates its OpenXR instance (early mod).
  passthrough::InstallXrHooks(loadSettings());
}

// Called later on in the game loading - a good time to install function hooks
MOD_EXTERN_FUNC void late_load() noexcept {
  il2cpp_functions::Init();

  PaperLogger.info("late_load() done");
}
