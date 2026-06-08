#include "demo_scene.hpp"

#include "renderer/gltf_loader.hpp"
#include "renderer/model_loader.hpp"
#include "renderer/pipeline_id.hpp"
#include "scene/floor_mesh.hpp"
#include "scene/mesh_instance.hpp"
#include "scene/scene.hpp"
#include "scene/sky_mesh.hpp"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace app {

namespace {

[[nodiscard]] auto asset_path(std::string_view relative) -> std::string {
  return std::string(APP_ASSETS_DIR) + std::string(relative);
}

[[nodiscard]] auto engine_asset_path(std::string_view relative) -> std::string {
  return std::string(APP_VCE_ASSETS) + std::string(relative);
}

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

} // namespace

void populate_demo_scene(engine::Scene &scene) {
  std::unordered_map<std::string, std::uint32_t> texture_cache;

  const engine::LoadedMesh room_loaded = engine::load_obj_model(engine_asset_path("/models/viking_room.obj"));
  const std::uint32_t room_mesh = scene.add_mesh({.vertices = room_loaded.vertices, .indices = room_loaded.indices});

  const engine::LoadedGltfModel susan_gltf = engine::load_gltf_model(asset_path("/models/susan.gltf"));
  const engine::LoadedGltfModel wolf_gltf = engine::load_gltf_model(asset_path("/models/wolf-thing.gltf"));
  const engine::LoadedGltfModel sphere_glb = engine::load_gltf_model(asset_path("/models/sphere.glb"));
  const engine::LoadedGltfModel sphere_gltf = engine::load_gltf_model(asset_path("/models/sphere.gltf"));
  const engine::LoadedGltfModel suzanne_glb = engine::load_gltf_model(asset_path("/models/Suzanne.glb"));

  const std::uint32_t susan_gltf_mesh = add_gltf_mesh(scene, susan_gltf.primitives.front());
  const std::uint32_t wolf_mesh = add_gltf_mesh(scene, wolf_gltf.primitives.front());
  const std::uint32_t suzanne_glb_mesh = add_gltf_mesh(scene, suzanne_glb.primitives.front());

  const engine::LoadedMesh torus_loaded = engine::load_obj_model(asset_path("/models/Torus.obj"));
  const std::uint32_t torus_mesh = scene.add_mesh({.vertices = torus_loaded.vertices, .indices = torus_loaded.indices});

  const std::uint32_t sky_mesh = scene.add_mesh(engine::make_sky_cube_mesh());
  const std::uint32_t floor_mesh = scene.add_mesh(engine::make_floor_quad_mesh(25.0F));

  const std::uint32_t viking_texture = add_cached_texture(scene, texture_cache, engine_asset_path("/textures/viking_room.png"));
  const std::uint32_t dirt_table_texture = add_cached_texture(scene, texture_cache, asset_path("/textures/tiles/dirt.png"));
  const std::uint32_t susan_gltf_texture = texture_for_gltf_material(scene, texture_cache, susan_gltf.primitives.front().material);
  const std::uint32_t wolf_texture = texture_for_gltf_material(scene, texture_cache, wolf_gltf.primitives.front().material);
  const std::uint32_t suzanne_glb_texture = texture_for_gltf_material(scene, texture_cache, suzanne_glb.primitives.front().material);

  std::uint32_t brick_array_layer{};
  for (const char *layer_name : tile_array_layers) {
    const std::uint32_t layer = scene.add_texture_array_layer(asset_path("/textures/tiles/") + layer_name);
    if (std::string_view(layer_name) == "brick.png")
      brick_array_layer = layer;
  }

  scene.camera().look_at({4.0F, 2.0F, 6.0F}, {0.0F, 0.0F, 0.0F});
  scene.camera().set_perspective(45.0F, 0.05F, 2000.0F);

  scene.directional_light().direction_toward_light = glm::normalize(glm::vec3(0.4F, 1.0F, 0.3F));
  scene.directional_light().color = glm::vec3(1.0F, 0.98F, 0.92F);
  scene.directional_light().intensity = 1.0F;
  scene.directional_light().ambient = 0.18F;

  (void)scene.add_instance({
      .mesh_index = sky_mesh,
      .model = glm::scale(glm::mat4(1.0F), glm::vec3(500.0F)),
      .layer = engine::RenderLayer::Background,
      .pipeline = engine::PipelineId::Background,
  });

  (void)scene.add_instance({
      .mesh_index = floor_mesh,
      .texture_index = dirt_table_texture,
      .texture_source = engine::TextureSource::Table,
      .model = glm::mat4(1.0F),
      .layer = engine::RenderLayer::Opaque,
  });

  (void)scene.add_instance({
      .mesh_index = room_mesh,
      .texture_index = viking_texture,
      .texture_source = engine::TextureSource::Table,
      .model = glm::mat4(1.0F),
      .layer = engine::RenderLayer::Opaque,
  });

  (void)scene.add_instance({
      .mesh_index = susan_gltf_mesh,
      .texture_index = susan_gltf_texture,
      .texture_source = engine::TextureSource::Table,
      .model = glm::translate(glm::mat4(1.0F), glm::vec3(-2.5F, 0.0F, -1.0F)) *
               glm::scale(glm::mat4(1.0F), glm::vec3(0.5F)) *
               susan_gltf.primitives.front().node_transform,
      .layer = engine::RenderLayer::Opaque,
  });

  (void)scene.add_instance({
      .mesh_index = wolf_mesh,
      .texture_index = wolf_texture,
      .texture_source = engine::TextureSource::Table,
      .model = glm::translate(glm::mat4(1.0F), glm::vec3(2.5F, 1.0F, -1.0F)) *
               glm::scale(glm::mat4(1.0F), glm::vec3(0.8F)) *
               wolf_gltf.primitives.front().node_transform,
      .layer = engine::RenderLayer::Opaque,
  });

  add_gltf_model_instances(
      scene,
      texture_cache,
      sphere_glb,
      glm::translate(glm::mat4(1.0F), glm::vec3(-0.8F, 2.4F, -2.0F)) * glm::scale(glm::mat4(1.0F), glm::vec3(0.9F)));

  add_gltf_model_instances(
      scene,
      texture_cache,
      sphere_gltf,
      glm::translate(glm::mat4(1.0F), glm::vec3(1.6F, 2.4F, -2.0F)) * glm::scale(glm::mat4(1.0F), glm::vec3(0.9F)));

  (void)scene.add_instance({
      .mesh_index = suzanne_glb_mesh,
      .texture_index = suzanne_glb_texture,
      .texture_source = engine::TextureSource::Table,
      .model = glm::translate(glm::mat4(1.0F), glm::vec3(-1.0F, 1.5F, 1.5F)) *
               glm::scale(glm::mat4(1.0F), glm::vec3(0.35F)) *
               suzanne_glb.primitives.front().node_transform,
      .layer = engine::RenderLayer::Opaque,
  });

  (void)scene.add_instance({
      .mesh_index = torus_mesh,
      .texture_index = brick_array_layer,
      .texture_source = engine::TextureSource::ArrayLayer,
      .model = glm::translate(glm::mat4(1.0F), glm::vec3(1.5F, 1.0F, 1.5F)) *
               glm::scale(glm::mat4(1.0F), glm::vec3(0.8F)),
      .layer = engine::RenderLayer::Opaque,
  });

  (void)scene.add_instance({
      .mesh_index = room_mesh,
      .texture_index = dirt_table_texture,
      .texture_source = engine::TextureSource::Table,
      .model = glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, -0.5F, 0.0F)) *
               glm::scale(glm::mat4(1.0F), glm::vec3(0.25F)),
      .layer = engine::RenderLayer::Opaque,
  });
}

auto create_demo_scene() -> engine::Scene {
  engine::Scene scene;
  populate_demo_scene(scene);
  return scene;
}

void update_demo_scene(engine::Scene &scene) {
  static const auto start_time = std::chrono::high_resolution_clock::now();
  const auto current_time = std::chrono::high_resolution_clock::now();
  const float time = std::chrono::duration<float>(current_time - start_time).count();

  if (scene.instances().size() >= 12)
    scene.instance(11).model = glm::rotate(glm::mat4(1.0F), time * glm::radians(90.0F), glm::vec3(0.0F, 1.0F, 0.0F)) *
                               glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, -0.5F, 0.0F)) *
                               glm::scale(glm::mat4(1.0F), glm::vec3(0.25F));
}

} // namespace app
