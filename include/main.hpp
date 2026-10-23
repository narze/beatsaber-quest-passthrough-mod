#pragma once

// Include the modloader header, which allows us to tell the modloader which mod
// this is, and the version etc.
#include "scotland2/shared/modloader.h"

#include "beatsaber-hook/shared/utils/hooking.hpp"
#include "beatsaber-hook/shared/utils/il2cpp-functions.hpp"

#include "paper2_scotland2/shared/logger.hpp"

#include "_config.hpp"

constexpr auto PaperLogger = Paper::ConstLoggerContext("Passthrough");
