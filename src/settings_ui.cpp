// In-game settings page (Settings > Mod Settings > Passthrough), built with
// BSML-Lite. Every change is applied at once and saved to the config file.

#include "settings_ui.hpp"

#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML.hpp"
#include "config.hpp"
#include "environment.hpp"
#include "main.hpp"
#include "settings.hpp"

namespace {

using passthrough::CurrentSettings;
using passthrough::Settings;

// Applies a change: update the live settings, save, and re-apply the
// environment (the OpenXR hooks read the settings on every frame).
template <typename F> void change(F &&edit) {
  CurrentSettings().Update(edit);
  passthrough::SaveConfig();
  passthrough::ApplyEnvironment();
}

void didActivate(HMUI::ViewController *self, bool firstActivation, bool, bool) {
  if (!firstActivation) return;
  auto container = BSML::Lite::CreateScrollableSettingsContainer(self->get_transform());
  const Settings s = CurrentSettings().Get();

  BSML::Lite::CreateToggle(container, "Passthrough", s.enabled,
                           [](bool v) { change([v](Settings &x) { x.enabled = v; }); });
  BSML::Lite::CreateToggle(container, "Hide song environment", s.hideEnvironment,
                           [](bool v) { change([v](Settings &x) { x.hideEnvironment = v; }); });
  BSML::Lite::CreateToggle(container, "Hide menu environment", s.hideMenuEnvironment,
                           [](bool v) { change([v](Settings &x) { x.hideMenuEnvironment = v; }); });
  // How dark a pixel must be to show the room. Higher = more see-through.
  BSML::Lite::CreateSliderSetting(container, "See-through below brightness", 0.005f, s.keyHigh,
                                  0.005f, 0.2f, [](float v) {
                                    change([v](Settings &x) {
                                      x.keyLow = 0.0f;
                                      x.keyHigh = v;
                                    });
                                  });
  BSML::Lite::CreateSliderSetting(container, "Room brightness (opacity)", 0.05f, s.opacity, 0.0f,
                                  1.0f, [](float v) { change([v](Settings &x) { x.opacity = v; }); });
}

} // namespace

namespace passthrough {

void RegisterSettingsUi() {
  BSML::Init();
  BSML::Register::RegisterSettingsMenu("Passthrough", didActivate, false);
}

} // namespace passthrough
