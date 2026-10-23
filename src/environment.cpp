// Hides the game environment, so the passthrough layer is the background.
//
// Beat Saber loads each environment as its own scene ("DefaultEnvironment",
// ...). We disable the Renderers of those scenes but leave their GameObjects
// active: lighting and event scripts keep their references and keep running,
// they just draw nothing. The cleared background is black, which the alpha
// key turns into passthrough. We remember what we disabled, so a settings
// change can show it again without a restart.

#include "environment.hpp"

#include <string>
#include <vector>

#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/SceneManagement/LoadSceneMode.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/SceneManagement/SceneManager.hpp"
#include "UnityEngine/Transform.hpp"
#include "beatsaber-hook/shared/utils/typedefs-wrappers.hpp"
#include "main.hpp"
#include "scene_filter.hpp"
#include "settings.hpp"

namespace {

using UnityEngine::Renderer;
using UnityEngine::SceneManagement::Scene;

struct HiddenRenderer {
  SafePtrUnity<Renderer> renderer;
  std::string scene;
};
std::vector<HiddenRenderer> hidden; // main thread only

bool shouldHide(const std::string &scene, const passthrough::Settings &cfg) {
  return cfg.enabled && cfg.hideEnvironment &&
         passthrough::IsEnvironmentScene(scene, cfg.hideMenuEnvironment);
}

// The main menu environment is not its own scene: it lives in "MainMenu"
// under these objects. We track it under a pseudo-scene name, so the
// filter treats it as a menu environment.
constexpr const char *kMenuScene = "MainMenu";
constexpr const char *kMenuEnvironment = "MainMenuEnvironment";
constexpr const char *kMenuEnvironmentObjects[] = {"Wrapper/MenuEnvironmentManager",
                                                   "Wrapper/MenuEnvironmentCore"};

int hideUnder(UnityEngine::GameObject *object, const std::string &trackAs) {
  int count = 0;
  for (auto renderer : object->GetComponentsInChildren<Renderer *>(true)) {
    if (renderer && renderer->get_enabled()) {
      renderer->set_enabled(false);
      hidden.push_back({SafePtrUnity<Renderer>(renderer), trackAs});
      count++;
    }
  }
  return count;
}

void hideScene(Scene scene, const std::string &name) {
  int count = 0;
  for (auto root : scene.GetRootGameObjects()) {
    if (root) count += hideUnder(root, name);
  }
  PaperLogger.info("scene {}: hid {} renderers", name, count);
}

void hideMenuEnvironment(Scene menu) {
  int count = 0;
  for (auto root : menu.GetRootGameObjects()) {
    if (!root) continue;
    for (auto path : kMenuEnvironmentObjects) {
      std::string_view p(path);
      auto slash = p.find('/');
      if (std::string(root->get_name()) != p.substr(0, slash)) continue;
      auto child = root->get_transform()->Find(std::string(p.substr(slash + 1)));
      if (child) count += hideUnder(child->get_gameObject(), kMenuEnvironment);
    }
  }
  PaperLogger.info("menu environment: hid {} renderers", count);
}

// Debug aid: the main menu environment is not a separate scene, so log the
// menu structure to find it.
void logRoots(Scene scene, const std::string &name) {
  for (auto root : scene.GetRootGameObjects()) {
    if (!root) continue;
    std::string children;
    auto t = root->get_transform();
    for (int i = 0; i < t->get_childCount(); i++) {
      children += std::string(t->GetChild(i)->get_gameObject()->get_name()) + ", ";
    }
    PaperLogger.info("  {} root {} (active {}): {}", name, std::string(root->get_name()),
                     root->get_activeSelf(), children);
    for (auto path : kMenuEnvironmentObjects) {
      std::string_view p(path);
      if (std::string(root->get_name()) != p.substr(0, p.find('/'))) continue;
      auto child = t->Find(std::string(p.substr(p.find('/') + 1)));
      if (!child) continue;
      std::string names;
      for (int i = 0; i < child->get_childCount(); i++) {
        names += std::string(child->GetChild(i)->get_gameObject()->get_name()) + ", ";
      }
      PaperLogger.info("    {}: {}", path, names);
    }
  }
}

} // namespace

MAKE_HOOK_MATCH(SceneManager_Internal_SceneLoaded,
                &UnityEngine::SceneManagement::SceneManager::Internal_SceneLoaded, void, Scene scene,
                UnityEngine::SceneManagement::LoadSceneMode mode) {
  SceneManager_Internal_SceneLoaded(scene, mode);
  std::string name = scene.get_name();
  bool hide = shouldHide(name, passthrough::CurrentSettings().Get());
  PaperLogger.info("scene loaded: {} (hide: {})", name, hide);
  if (name == kMenuScene) {
    logRoots(scene, name);
    if (shouldHide(kMenuEnvironment, passthrough::CurrentSettings().Get())) hideMenuEnvironment(scene);
  }
  if (hide) hideScene(scene, name);
}

namespace passthrough {

void InstallEnvironmentHooks() { INSTALL_HOOK(PaperLogger, SceneManager_Internal_SceneLoaded); }

void ApplyEnvironment() {
  const Settings cfg = CurrentSettings().Get();

  // Show again what should no longer be hidden; forget destroyed renderers.
  // (SafePtrUnity cannot be assigned, so build a new list instead of erase_if.)
  int shown = 0;
  std::vector<HiddenRenderer> keep;
  for (auto &h : hidden) {
    if (!h.renderer) continue;
    if (shouldHide(h.scene, cfg)) {
      keep.push_back(h);
      continue;
    }
    h.renderer->set_enabled(true);
    shown++;
  }
  hidden.clear();
  for (auto &h : keep) hidden.push_back(h);

  // Hide loaded scenes that should be hidden now.
  for (int i = 0; i < UnityEngine::SceneManagement::SceneManager::get_sceneCount(); i++) {
    Scene scene = UnityEngine::SceneManagement::SceneManager::GetSceneAt(i);
    if (!scene.get_isLoaded()) continue;
    std::string name = scene.get_name();
    if (shouldHide(name, cfg)) hideScene(scene, name);
    if (name == kMenuScene && shouldHide(kMenuEnvironment, cfg)) hideMenuEnvironment(scene);
  }
  PaperLogger.info("environment applied: showed {} renderers, {} hidden now", shown, hidden.size());
}

} // namespace passthrough
