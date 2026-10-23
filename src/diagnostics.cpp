// Debug dump of what draws the picture: cameras (with their components) and
// every enabled renderer, grouped by scene and shader. Used to find the
// environment parts that are not plain renderers in environment scenes.

#include "diagnostics.hpp"

#include <map>
#include <string>

#include "UnityEngine/Behaviour.hpp"
#include "UnityEngine/Camera.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/Component.hpp"
#include "UnityEngine/GameObject.hpp"
#include "UnityEngine/Material.hpp"
#include "UnityEngine/Object.hpp"
#include "UnityEngine/Renderer.hpp"
#include "UnityEngine/SceneManagement/Scene.hpp"
#include "UnityEngine/Shader.hpp"
#include "main.hpp"

namespace {

std::string typeName(Il2CppObject *obj) {
  if (!obj) return "null";
  return il2cpp_functions::class_get_name(il2cpp_functions::object_get_class(obj));
}

} // namespace

namespace passthrough {

void DumpRenderState(const char *reason) {
  PaperLogger.info("=== render dump ({}) ===", reason);

  for (auto camera : UnityEngine::Camera::get_allCameras()) {
    if (!camera) continue;
    auto color = camera->get_backgroundColor();
    std::string components;
    for (auto c : camera->get_gameObject()->GetComponents<UnityEngine::Component *>()) {
      std::string name = typeName(reinterpret_cast<Il2CppObject *>(c));
      auto behaviour = il2cpp_utils::try_cast<UnityEngine::Behaviour>(c);
      if (behaviour) name += (*behaviour)->get_enabled() ? "(on)" : "(off)";
      components += name + ", ";
    }
    PaperLogger.info("camera {}: clearFlags {} bg ({:.2f},{:.2f},{:.2f},{:.2f}) mask 0x{:x} depth {} "
                     "components: {}",
                     std::string(camera->get_name()), static_cast<int>(camera->get_clearFlags()),
                     color.r, color.g, color.b, color.a, camera->get_cullingMask(),
                     camera->get_depth(), components);
  }

  // scene -> "shader / renderer type" -> count, plus a few object names
  std::map<std::string, std::map<std::string, std::pair<int, std::string>>> groups;
  for (auto r : UnityEngine::Object::FindObjectsOfType<UnityEngine::Renderer *>()) {
    if (!r || !r->get_enabled() || !r->get_gameObject()->get_activeInHierarchy()) continue;
    std::string scene = r->get_gameObject()->get_scene().get_name();
    auto material = r->get_sharedMaterial();
    std::string shader = material && material->get_shader()
                             ? std::string(material->get_shader()->get_name())
                             : "no-material";
    auto &entry = groups[scene][fmt::format("L{} ", r->get_gameObject()->get_layer()) +
                                typeName(reinterpret_cast<Il2CppObject *>(r)) + " / " + shader];
    if (entry.first++ < 3) entry.second += std::string(r->get_gameObject()->get_name()) + ", ";
  }
  for (auto &[scene, byShader] : groups) {
    for (auto &[kind, info] : byShader) {
      PaperLogger.info("renderer [{}] {} x{}: {}", scene, kind, info.first, info.second);
    }
  }
  PaperLogger.info("=== end render dump ===");
}

} // namespace passthrough
