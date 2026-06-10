#include "demo_anim.hpp"

#include "scene/scene.hpp"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <vector>

namespace app {

namespace {

struct DemoAnimTarget {
  SceneInstance instance;
  glm::mat4 base{1.0F};
  float speed_degrees{};
  glm::vec3 axis{0.0F, 1.0F, 0.0F};
};

std::vector<DemoAnimTarget> g_targets;

} // namespace

void demo_anim_add(SceneInstance instance, glm::mat4 base_transform, float speed_degrees, glm::vec3 axis) {
  g_targets.push_back(DemoAnimTarget{
      .instance = instance,
      .base = base_transform,
      .speed_degrees = speed_degrees,
      .axis = axis,
  });
}

void update_demo_animations(engine::Scene &scene) {
  static const auto start_time = std::chrono::high_resolution_clock::now();
  const auto current_time = std::chrono::high_resolution_clock::now();
  const float time = std::chrono::duration<float>(current_time - start_time).count();

  for (const DemoAnimTarget &target : g_targets) {
    const glm::mat4 model =
        glm::rotate(glm::mat4(1.0F), time * glm::radians(target.speed_degrees), target.axis) * target.base;
    target.instance.set_model(scene, model);
  }
}

} // namespace app
