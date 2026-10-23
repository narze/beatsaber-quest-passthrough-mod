#include "config.hpp"

#include <memory>
#include <string>

#include "beatsaber-hook/shared/config/config-utils.hpp"
#include "main.hpp"

// Configs/Passthrough.json keys:
//   enabled:     master switch
//   placement:   "underlay" (background) or "overlay" (on top, for testing)
//   opacity:     0..1, passthrough opacity
//   sourceAlpha: blend the game over passthrough with the game's alpha
//   lumaKey:     replace the game's alpha with a brightness key (keyLow..keyHigh)
//   hideEnvironment / hideMenuEnvironment: hide the environment renderers
//   distanceFog: keep the bloom fog when the environment is hidden
//   quickToggle: A+B on the right controller toggles passthrough in songs

namespace passthrough {

namespace {

std::unique_ptr<Configuration> config;

void writeSettings(ConfigDocument &doc, const Settings &s) {
  auto &alloc = doc.GetAllocator();
  doc.SetObject();
  doc.AddMember("enabled", s.enabled, alloc);
  const char *placement = s.placement == LayerPlacement::Overlay ? "overlay" : "underlay";
  doc.AddMember("placement", rapidjson::Value(placement, alloc).Move(), alloc);
  doc.AddMember("opacity", s.opacity, alloc);
  doc.AddMember("sourceAlpha", s.sourceAlpha, alloc);
  doc.AddMember("lumaKey", s.lumaKey, alloc);
  doc.AddMember("keyLow", s.keyLow, alloc);
  doc.AddMember("keyHigh", s.keyHigh, alloc);
  doc.AddMember("hideEnvironment", s.hideEnvironment, alloc);
  doc.AddMember("hideMenuEnvironment", s.hideMenuEnvironment, alloc);
  doc.AddMember("distanceFog", s.distanceFog, alloc);
  doc.AddMember("quickToggle", s.quickToggle, alloc);
}

} // namespace

void LoadConfig(const modloader::ModInfo &info) {
  config = std::make_unique<Configuration>(info);
  config->Load();
  auto &doc = config->config;
  Settings s; // defaults for anything missing or of the wrong type
  if (doc.IsObject()) {
    auto boolean = [&](const char *name, bool &value) {
      if (doc.HasMember(name) && doc[name].IsBool()) value = doc[name].GetBool();
    };
    auto number = [&](const char *name, float &value) {
      if (doc.HasMember(name) && doc[name].IsNumber()) value = doc[name].GetFloat();
    };
    boolean("enabled", s.enabled);
    if (doc.HasMember("placement") && doc["placement"].IsString()) {
      s.placement = std::string(doc["placement"].GetString()) == "overlay" ? LayerPlacement::Overlay
                                                                           : LayerPlacement::Underlay;
    }
    number("opacity", s.opacity);
    boolean("sourceAlpha", s.sourceAlpha);
    boolean("lumaKey", s.lumaKey);
    number("keyLow", s.keyLow);
    number("keyHigh", s.keyHigh);
    boolean("hideEnvironment", s.hideEnvironment);
    boolean("hideMenuEnvironment", s.hideMenuEnvironment);
    boolean("distanceFog", s.distanceFog);
    boolean("quickToggle", s.quickToggle);
  }
  CurrentSettings().Set(s);
  SaveConfig(); // normalise: every key present, values sanitized

  const Settings c = CurrentSettings().Get();
  PaperLogger.info("settings: enabled={} placement={} opacity={} sourceAlpha={} lumaKey={} "
                   "key={}..{} hideEnvironment={} hideMenuEnvironment={}",
                   c.enabled, c.placement == LayerPlacement::Overlay ? "overlay" : "underlay",
                   c.opacity, c.sourceAlpha, c.lumaKey, c.keyLow, c.keyHigh, c.hideEnvironment,
                   c.hideMenuEnvironment);
}

void SaveConfig() {
  if (!config) return;
  writeSettings(config->config, CurrentSettings().Get());
  config->Write();
}

} // namespace passthrough
