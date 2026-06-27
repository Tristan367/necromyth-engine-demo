#pragma once

#include "scene/scene.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace app {

struct PhysicsObjDesc {
  std::uint32_t instance_index;
  glm::vec3 box_half_extent{0.5F, 0.5F, 0.5F};
};

void populate_demo_scene(engine::Scene &scene);
[[nodiscard]] auto create_demo_scene(
    std::vector<PhysicsObjDesc> *out_obj_descs = nullptr,
    std::uint32_t *out_char_instance = nullptr,
    engine::MeshSource *out_trimesh_mesh = nullptr) -> engine::Scene;
void update_demo_scene(engine::Scene &scene);
void toggle_demo_animation(engine::Scene &scene);

} // namespace app
