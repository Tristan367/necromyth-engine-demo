#pragma once

#include "scene/mesh_source.hpp"

#include <glm/mat4x4.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>

namespace engine {
class Scene;
struct LoadedGltfModel;
}

namespace app {

[[nodiscard]] auto asset_path(std::string_view relative) -> std::string;

[[nodiscard]] auto lifted(glm::vec3 position) -> glm::mat4;
[[nodiscard]] auto lifted(glm::mat4 transform) -> glm::mat4;

struct DemoMeshLibrary {
  std::uint32_t susan_gltf_mesh{};
  std::uint32_t wolf_mesh{};
  std::uint32_t suzanne_glb_mesh{};
  std::uint32_t torus_mesh{};
  std::uint32_t sky_mesh{};
  std::uint32_t floor_mesh{};

  std::uint32_t dirt_table_texture{};
  std::uint32_t susan_gltf_texture{};
  std::uint32_t wolf_texture{};
  std::uint32_t suzanne_glb_texture{};
  std::uint32_t brick_array_layer{};

  glm::mat4 susan_node_transform{1.0F};
  glm::mat4 wolf_node_transform{1.0F};
  glm::mat4 suzanne_node_transform{1.0F};
};

[[nodiscard]] auto load_demo_mesh_library(engine::Scene &scene) -> DemoMeshLibrary;

void add_gltf_model_instances(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &texture_cache,
    const engine::LoadedGltfModel &model,
    glm::mat4 instance_transform);

void add_demo_sphere_instances(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &texture_cache);

void add_animation_test_model(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &texture_cache);

struct TrimeshTestData {
  engine::MeshSource mesh;
  std::string texture_path;
};

[[nodiscard]] auto load_trimesh_test_data() -> TrimeshTestData;

} // namespace app
