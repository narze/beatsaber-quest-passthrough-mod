// In-game settings, built with BSML-Lite. The same panel appears in four
// places:
//   - Settings > Mod Settings > Passthrough
//   - the "Passthrough" button in the main menu's mod list (left side)
//   - the "Passthrough" tab in the gameplay setup panel (song select)
//   - a floating panel next to the pause menu, to change settings mid-song
// Every change is applied at once and saved. Each panel refreshes its values
// when it is shown, so a change in one place shows in the others.

#include "settings_ui.hpp"

#include <optional>
#include <list>

#include "GlobalNamespace/AudioTimeSyncController.hpp"
#include "GlobalNamespace/OVRInput.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Transform.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"
#include "bsml/shared/BSML-Lite.hpp"
#include "bsml/shared/BSML.hpp"
#include "bsml/shared/BSML/Components/Settings/SliderSetting.hpp"
#include "bsml/shared/BSML/Components/Settings/ToggleSetting.hpp"
#include "bsml/shared/BSML/FloatingScreen/FloatingScreen.hpp"
#include "button_combo.hpp"
#include "config.hpp"
#include "environment.hpp"
#include "main.hpp"
#include "settings.hpp"

namespace {

using passthrough::CurrentSettings;
using passthrough::Settings;

bool refreshing = false; // true while panels show new values: ignore callbacks

// Applies a change: update the live settings, save, and re-apply the
// environment (the OpenXR hooks read the settings on every frame).
template <typename F> void change(F &&edit) {
  if (refreshing) return;
  CurrentSettings().Update(edit);
  passthrough::SaveConfig();
  passthrough::ApplyEnvironment();
}

struct Panel {
  SafePtrUnity<BSML::ToggleSetting> enabled, hideSong, hideMenu, fog, quickToggle, showMask;
  SafePtrUnity<BSML::SliderSetting> key, opacity;

  void Refresh() const {
    if (!enabled) return; // destroyed with its scene
    const Settings s = CurrentSettings().Get();
    refreshing = true;
    enabled->set_Value(s.enabled);
    if (hideSong) hideSong->set_Value(s.hideEnvironment);
    if (hideMenu) hideMenu->set_Value(s.hideMenuEnvironment);
    if (fog) fog->set_Value(s.distanceFog);
    if (quickToggle) quickToggle->set_Value(s.quickToggle);
    if (key) key->set_Value(s.keyHigh);
    if (opacity) opacity->set_Value(s.opacity);
    if (showMask) showMask->set_Value(s.showMask);
    refreshing = false;
  }
};

std::list<Panel> panels; // main thread only (list: Panel is not assignable)

// Builds the controls into `parent`.
void buildPanel(UnityEngine::Transform *parent) {
  const Settings s = CurrentSettings().Get();
  auto enabled = BSML::Lite::CreateToggle(parent, "Passthrough", s.enabled, [](bool v) {
    change([v](Settings &x) { x.enabled = v; });
  });
  auto hideSong = BSML::Lite::CreateToggle(parent, "Hide song environment", s.hideEnvironment, [](bool v) {
    change([v](Settings &x) { x.hideEnvironment = v; });
  });
  auto hideMenu = BSML::Lite::CreateToggle(parent, "Hide menu environment", s.hideMenuEnvironment,
                                           [](bool v) { change([v](Settings &x) { x.hideMenuEnvironment = v; }); });
  auto fog = BSML::Lite::CreateToggle(parent, "Distance fog", s.distanceFog, [](bool v) {
    change([v](Settings &x) { x.distanceFog = v; });
  });
  auto quickToggle = BSML::Lite::CreateToggle(parent, "A+B toggles passthrough in songs", s.quickToggle,
                                              [](bool v) { change([v](Settings &x) { x.quickToggle = v; }); });
  // How dark a pixel must be to show the room. Higher = more see-through.
  auto key = BSML::Lite::CreateSliderSetting(parent, "See-through below brightness", 0.005f, s.keyHigh, 0.005f,
                                             0.2f, [](float v) {
                                               change([v](Settings &x) {
                                                 x.keyLow = 0.0f;
                                                 x.keyHigh = v;
                                               });
                                             });
  auto opacity = BSML::Lite::CreateSliderSetting(parent, "Room brightness (opacity)", 0.05f, s.opacity, 0.0f, 1.0f,
                                                 [](float v) { change([v](Settings &x) { x.opacity = v; }); });
  // Debug: white = game, black = room. Shows what the brightness key keeps.
  auto showMask = BSML::Lite::CreateToggle(parent, "Debug: show see-through mask", s.showMask, [](bool v) {
    change([v](Settings &x) { x.showMask = v; });
  });

  // Drop panels whose scene is gone, then remember this one.
  std::erase_if(panels, [](const Panel &p) { return !p.enabled; });
  panels.push_back(Panel{enabled, hideSong, hideMenu, fog, quickToggle, showMask, key, opacity});
}

void refreshAll() {
  for (auto &p : panels) p.Refresh();
}

// Shared by Mod Settings and the main menu button.
void viewControllerDidActivate(HMUI::ViewController *self, bool firstActivation, bool, bool) {
  if (firstActivation) {
    buildPanel(BSML::Lite::CreateScrollableSettingsContainer(self->get_transform())->get_transform());
  }
  refreshAll();
}

void gameplaySetupDidActivate(UnityEngine::GameObject *self, bool firstActivation) {
  if (firstActivation) {
    buildPanel(BSML::Lite::CreateScrollableSettingsContainer(self->get_transform())->get_transform());
  }
  refreshAll();
}

// --- pause menu panel ---

std::optional<SafePtrUnity<BSML::FloatingScreen>> pauseScreen; // optional: not assignable

void showPausePanel(bool show) {
  if (show && (!pauseScreen || !*pauseScreen)) {
    // Left of the pause menu, turned towards the player. 100 wide: the
    // settings rows need it (70 clipped the labels and the slider values).
    // The right edge stays where it was, clear of the pause menu.
    auto screen = BSML::Lite::CreateFloatingScreen({100, 60}, {-1.70f, 1.35f, 1.43f}, {0, -35, 0}, 0.0f, true, false);
    auto container = BSML::Lite::CreateScrollableSettingsContainer(screen->get_transform());
    buildPanel(container->get_transform());
    pauseScreen.emplace(screen);
  }
  if (pauseScreen && *pauseScreen) {
    (*pauseScreen)->get_gameObject()->SetActive(show);
    if (show) refreshAll();
  }
}

passthrough::ButtonCombo quickToggleCombo; // main thread only

} // namespace

// The song clock updates every frame, only in songs: check the A+B combo.
MAKE_HOOK_MATCH(AudioTimeSyncController_Update, &GlobalNamespace::AudioTimeSyncController::Update, void,
                GlobalNamespace::AudioTimeSyncController *self) {
  AudioTimeSyncController_Update(self);
  using GlobalNamespace::OVRInput;
  auto right = OVRInput::Controller::RTouch;
  bool fire = quickToggleCombo.Update(OVRInput::Get(OVRInput::RawButton::A, right),
                                      OVRInput::Get(OVRInput::RawButton::B, right));
  if (!fire || !CurrentSettings().Get().quickToggle) return;
  change([](Settings &x) { x.enabled = !x.enabled; });
  refreshAll();
  PaperLogger.info("A+B: passthrough {}", CurrentSettings().Get().enabled ? "on" : "off");
}

MAKE_HOOK_MATCH(PauseMenuManager_ShowMenu, &GlobalNamespace::PauseMenuManager::ShowMenu, void,
                GlobalNamespace::PauseMenuManager *self) {
  PauseMenuManager_ShowMenu(self);
  showPausePanel(true);
}

MAKE_HOOK_MATCH(PauseMenuManager_StartResumeAnimation, &GlobalNamespace::PauseMenuManager::StartResumeAnimation,
                void, GlobalNamespace::PauseMenuManager *self) {
  showPausePanel(false);
  PauseMenuManager_StartResumeAnimation(self);
}

MAKE_HOOK_MATCH(PauseMenuManager_MenuButtonPressed, &GlobalNamespace::PauseMenuManager::MenuButtonPressed, void,
                GlobalNamespace::PauseMenuManager *self) {
  showPausePanel(false);
  PauseMenuManager_MenuButtonPressed(self);
}

MAKE_HOOK_MATCH(PauseMenuManager_RestartButtonPressed, &GlobalNamespace::PauseMenuManager::RestartButtonPressed,
                void, GlobalNamespace::PauseMenuManager *self) {
  showPausePanel(false);
  PauseMenuManager_RestartButtonPressed(self);
}

namespace passthrough {

void RegisterSettingsUi() {
  BSML::Init();
  BSML::Register::RegisterSettingsMenu("Passthrough", viewControllerDidActivate, false);
  BSML::Register::RegisterMainMenuViewControllerMethod("Passthrough", "Passthrough",
                                                       "See your room instead of the environment",
                                                       viewControllerDidActivate);
  BSML::Register::RegisterGameplaySetupTab("Passthrough", gameplaySetupDidActivate);
  INSTALL_HOOK(PaperLogger, AudioTimeSyncController_Update);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_ShowMenu);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_StartResumeAnimation);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_MenuButtonPressed);
  INSTALL_HOOK(PaperLogger, PauseMenuManager_RestartButtonPressed);
}

} // namespace passthrough
