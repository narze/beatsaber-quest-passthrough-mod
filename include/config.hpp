#pragma once

#include "scotland2/shared/loader.hpp"
#include "settings.hpp"

namespace passthrough {

// Reads Configs/Passthrough.json into CurrentSettings(), and writes back any
// missing keys with their defaults, so the file always shows every setting.
void LoadConfig(const modloader::ModInfo &info);

// Writes CurrentSettings() to Configs/Passthrough.json.
void SaveConfig();

} // namespace passthrough
