#pragma once

#include "scene_instance.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace engine {
class Scene;
}

namespace app {

void demo_anim_add(SceneInstance instance, glm::mat4 base_transform, float speed_degrees, glm::vec3 axis);

void update_demo_animations(engine::Scene &scene);

} // namespace app
