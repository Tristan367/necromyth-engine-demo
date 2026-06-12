#include "demo_assets.hpp"

#include "demo_meshes.hpp"
#include "renderer/gltf_loader.hpp"
#include "renderer/model_loader.hpp"
#include "scene/mesh_instance.hpp"
#include "scene/scene.hpp"
#include "scene/sky_mesh.hpp"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace app {

namespace {

constexpr float k_scene_lift_y = 1.0F;

constexpr auto tile_array_layers = std::array{
    "brick.png",
    "dirt.png",
    "concrete.png",
    "bark0.png",
    "ceiling0.png",
    "grass.png",
};

[[nodiscard]] auto add_cached_texture(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &cache,
    std::string path) -> std::uint32_t {
  if (const auto iterator = cache.find(path); iterator != cache.end())
    return iterator->second;

  const std::string path_key = path;
  const std::uint32_t index = scene.add_texture(std::move(path));
  cache.emplace(path_key, index);
  return index;
}

[[nodiscard]] auto add_gltf_mesh(engine::Scene &scene, const engine::LoadedGltfPrimitive &primitive) -> std::uint32_t {
  return scene.add_mesh({.vertices = primitive.mesh.vertices, .indices = primitive.mesh.indices});
}

[[nodiscard]] auto texture_for_gltf_material(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &cache,
    const engine::LoadedGltfMaterial &material) -> std::uint32_t {
  if (!material.base_color_texture_path)
    throw std::runtime_error("glTF primitive is missing a base color texture path");

  return add_cached_texture(scene, cache, *material.base_color_texture_path);
}

} // namespace

[[nodiscard]] auto asset_path(std::string_view relative) -> std::string {
  return std::string(APP_ASSETS_DIR) + std::string(relative);
}

[[nodiscard]] auto lifted(glm::vec3 position) -> glm::mat4 {
  return glm::translate(glm::mat4(1.0F), glm::vec3(position.x, position.y + k_scene_lift_y, position.z));
}

[[nodiscard]] auto lifted(glm::mat4 transform) -> glm::mat4 {
  return glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, k_scene_lift_y, 0.0F)) * transform;
}

void add_gltf_model_instances(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &texture_cache,
    const engine::LoadedGltfModel &model,
    glm::mat4 instance_transform) {
  for (const engine::LoadedGltfPrimitive &primitive : model.primitives) {
    const std::uint32_t mesh_index = add_gltf_mesh(scene, primitive);
    const std::uint32_t texture_index = texture_for_gltf_material(scene, texture_cache, primitive.material);

    (void)scene.add_instance({
        .mesh_index = mesh_index,
        .texture_index = texture_index,
        .texture_source = engine::TextureSource::Table,
        .model = instance_transform * primitive.node_transform,
        .layer = engine::RenderLayer::Opaque,
    });
  }
}

[[nodiscard]] auto load_demo_mesh_library(engine::Scene &scene) -> DemoMeshLibrary {
  std::unordered_map<std::string, std::uint32_t> texture_cache;

  DemoMeshLibrary library{};

  const engine::LoadedGltfModel susan_gltf = engine::load_gltf_model(asset_path("/models/susan.gltf"));
  const engine::LoadedGltfModel wolf_gltf = engine::load_gltf_model(asset_path("/models/wolf-thing.gltf"));
  const engine::LoadedGltfModel suzanne_glb = engine::load_gltf_model(asset_path("/models/Suzanne.glb"));

  library.susan_node_transform = susan_gltf.primitives.front().node_transform;
  library.wolf_node_transform = wolf_gltf.primitives.front().node_transform;
  library.suzanne_node_transform = suzanne_glb.primitives.front().node_transform;

  library.susan_gltf_mesh = add_gltf_mesh(scene, susan_gltf.primitives.front());
  library.wolf_mesh = add_gltf_mesh(scene, wolf_gltf.primitives.front());
  library.suzanne_glb_mesh = add_gltf_mesh(scene, suzanne_glb.primitives.front());

  const engine::LoadedMesh torus_loaded = engine::load_obj_model(asset_path("/models/Torus.obj"));
  library.torus_mesh = scene.add_mesh({.vertices = torus_loaded.vertices, .indices = torus_loaded.indices});

  library.sky_mesh = scene.add_mesh(engine::make_sky_cube_mesh());
  library.floor_mesh = scene.add_mesh(make_floor_quad_mesh(25.0F));

  library.dirt_table_texture = add_cached_texture(scene, texture_cache, asset_path("/textures/tiles/dirt.png"));
  library.susan_gltf_texture = texture_for_gltf_material(scene, texture_cache, susan_gltf.primitives.front().material);
  library.wolf_texture = texture_for_gltf_material(scene, texture_cache, wolf_gltf.primitives.front().material);
  library.suzanne_glb_texture =
      texture_for_gltf_material(scene, texture_cache, suzanne_glb.primitives.front().material);

  for (const char *layer_name : tile_array_layers) {
    const std::uint32_t layer = scene.add_texture_array_layer(asset_path("/textures/tiles/") + layer_name);
    if (std::string_view(layer_name) == "brick.png")
      library.brick_array_layer = layer;
  }

  (void)texture_cache;
  return library;
}

void add_demo_sphere_instances(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &texture_cache) {
  const engine::LoadedGltfModel sphere_glb = engine::load_gltf_model(asset_path("/models/sphere.glb"));
  const engine::LoadedGltfModel sphere_gltf = engine::load_gltf_model(asset_path("/models/sphere.gltf"));

  add_gltf_model_instances(
      scene,
      texture_cache,
      sphere_glb,
      lifted(glm::translate(glm::mat4(1.0F), glm::vec3(-0.8F, 2.4F, -2.0F)) * glm::scale(glm::mat4(1.0F), glm::vec3(0.9F))));

  add_gltf_model_instances(
      scene,
      texture_cache,
      sphere_gltf,
      lifted(glm::translate(glm::mat4(1.0F), glm::vec3(1.6F, 2.4F, -2.0F)) * glm::scale(glm::mat4(1.0F), glm::vec3(0.9F))));
}

} // namespace app
