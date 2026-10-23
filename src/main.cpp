#include "main.hpp"

#include "scotland2/shared/modloader.h"
#include "xr_hooks.hpp"

// Stores the ID and version of our mod, and is sent to
// the modloader upon startup
static modloader::ModInfo modInfo{MOD_ID, VERSION, 0};

// Called at the early stages of game loading
MOD_EXTERN_FUNC void setup(CModInfo *info) noexcept {
  *info = modInfo.to_c();

  // File logging
  Paper::Logger::RegisterFileContextId(PaperLogger.tag);

  PaperLogger.info("Passthrough {} setup()", VERSION);

  // Must be before the game creates its OpenXR instance (early mod).
  passthrough::InstallXrHooks();
}

// Called later on in the game loading - a good time to install function hooks
MOD_EXTERN_FUNC void late_load() noexcept {
  il2cpp_functions::Init();

  PaperLogger.info("late_load() done");
}
