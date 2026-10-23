#pragma once

#include <string_view>

namespace passthrough {

// True for Unity scenes that hold only environment visuals: Beat Saber loads
// each one as "<Name>Environment" (DefaultEnvironment, GridEnvironment, ...).
// Menu environments ("...MenuEnvironment") match only when `includeMenu`.
inline bool IsEnvironmentScene(std::string_view name, bool includeMenu) {
  constexpr std::string_view suffix = "Environment";
  if (name.size() <= suffix.size() || !name.ends_with(suffix)) return false;
  bool menu = name.find("Menu") != std::string_view::npos;
  return !menu || includeMenu;
}

} // namespace passthrough
