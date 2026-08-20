#pragma once

#include "scene/scene.hpp"

#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace app {

enum class TestObjShape : std::uint8_t { Box, Sphere, Capsule, Cylinder, TaperedCapsule, TaperedCylinder };

struct PhysicsObjDesc {
  engine::InstanceHandle instance;
  TestObjShape shape;
  glm::vec3 p1{0.5F};  // Box: half_extent.  Sphere: radius.  Capsule/Cylinder: half_height
  float p2{0.5F};       // Capsule/Cylinder: radius.  Tapered: top_radius
  float p3{0.5F};       // Tapered: bottom_radius
};

void populate_demo_scene(engine::Scene &scene);
[[nodiscard]] auto create_demo_scene(
    std::vector<PhysicsObjDesc> *out_obj_descs = nullptr,
    engine::InstanceHandle *out_char_instance = nullptr,
    engine::MeshSource *out_trimesh_mesh = nullptr) -> engine::Scene;
void update_demo_scene(engine::Scene &scene);

} // namespace app
