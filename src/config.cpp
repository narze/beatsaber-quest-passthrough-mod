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
//   pausePanel:  position (x, y, z) and rotation (qx, qy, qz, qw) of the
//                pause menu panel; the user moves it with its handle

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
  rapidjson::Value pose(rapidjson::kObjectType);
  const PanelPose &p = s.pausePanel;
  pose.AddMember("x", p.x, alloc);
  pose.AddMember("y", p.y, alloc);
  pose.AddMember("z", p.z, alloc);
  pose.AddMember("qx", p.qx, alloc);
  pose.AddMember("qy", p.qy, alloc);
  pose.AddMember("qz", p.qz, alloc);
  pose.AddMember("qw", p.qw, alloc);
  doc.AddMember("pausePanel", pose, alloc);
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
    if (doc.HasMember("pausePanel") && doc["pausePanel"].IsObject()) {
      auto &pose = doc["pausePanel"];
      auto get = [&](const char *name, float &value) {
        if (pose.HasMember(name) && pose[name].IsNumber()) value = pose[name].GetFloat();
      };
      PanelPose &p = s.pausePanel;
      get("x", p.x);
      get("y", p.y);
      get("z", p.z);
      get("qx", p.qx);
      get("qy", p.qy);
      get("qz", p.qz);
      get("qw", p.qw);
    }
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
