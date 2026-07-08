#include "demo_assets.hpp"

#include "demo_meshes.hpp"
#include "renderer/gltf_loader.hpp"
#include "renderer/model_loader.hpp"
#include "scene/mesh_instance.hpp"
#include "scene/scene.hpp"
#include "scene/sky_mesh.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcpp"
#endif
#include <tinygltf/json.hpp>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
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
  if (!material.base_color_texture_path) {
    std::cerr << "Warning: glTF primitive missing base color texture, using fallback\n";
    return add_cached_texture(scene, cache, std::string(asset_path("/textures/gray.png")));
  }

  return add_cached_texture(scene, cache, *material.base_color_texture_path);
}

} // namespace

[[nodiscard]] auto asset_path(std::string_view relative) -> std::string {
  return std::string(APP_ASSETS_DIR) + std::string(relative);
}

namespace {

void load_model_metadata(const std::string &gltf_path, engine::SkeletonAsset &skeleton) {
  const std::filesystem::path p(gltf_path);
  const std::filesystem::path json_path =
      p.parent_path() / (p.stem().string() + ".json");
  if (!std::filesystem::exists(json_path))
    return;

  std::ifstream file(json_path);
  if (!file)
    return;

  const nlohmann::json root = nlohmann::json::parse(file, nullptr, false);
  if (root.is_discarded())
    return;

  auto resolve_bone = [&](const nlohmann::json &bone_ref) -> std::optional<std::uint32_t> {
    if (bone_ref.is_number_unsigned())
      return bone_ref.get<std::uint32_t>();
    if (bone_ref.is_string())
      return skeleton.find_joint_index(bone_ref.get<std::string>());
    return std::nullopt;
  };

  if (auto it = root.find("body"); it != root.end()) {
    const auto &body = *it;
    engine::BodyColliderDef def;
    if (body.find("half_height") != body.end()) def.half_height = body["half_height"].get<float>();
    if (body.find("radius") != body.end()) def.radius = body["radius"].get<float>();
    if (body.find("offset") != body.end() && body["offset"].is_array())
      def.offset = glm::vec3(body["offset"][0].get<float>(),
                             body["offset"][1].get<float>(),
                             body["offset"][2].get<float>());
    const std::string shape_str = body.value("shape", "capsule");
    if (shape_str == "cylinder") def.shape = engine::BodyColliderDef::Shape::Cylinder;
    else if (shape_str == "box") def.shape = engine::BodyColliderDef::Shape::Box;
    else if (shape_str == "sphere") def.shape = engine::BodyColliderDef::Shape::Sphere;
    skeleton.body_collider = def;
  }

  if (auto it = root.find("hitboxes"); it != root.end() && it->is_array()) {
    for (const auto &hb : *it) {
      if (hb.find("bone") == hb.end() || hb.find("shape") == hb.end())
        continue;

      auto bone_idx = resolve_bone(hb["bone"]);
      if (!bone_idx || *bone_idx >= skeleton.joint_nodes.size())
        continue;

      engine::HitboxAttachment a;
      a.name = hb.value("name", "");
      a.joint_index = *bone_idx;

      const std::string shape = hb["shape"].get<std::string>();
      if (shape == "box") a.shape = engine::HitboxShape::Box;
      else if (shape == "sphere") a.shape = engine::HitboxShape::Sphere;
      else a.shape = engine::HitboxShape::Capsule;

      if (hb.find("offset") != hb.end() && hb["offset"].is_array())
        a.offset = glm::vec3(hb["offset"][0].get<float>(),
                             hb["offset"][1].get<float>(),
                             hb["offset"][2].get<float>());

      if (hb.find("radius") != hb.end()) a.half_extent.x = hb["radius"].get<float>();
      else if (hb.find("half_extent") != hb.end() && hb["half_extent"].is_array())
        a.half_extent = glm::vec3(hb["half_extent"][0].get<float>(),
                                  hb["half_extent"][1].get<float>(),
                                  hb["half_extent"][2].get<float>());

      if (hb.find("half_height") != hb.end()) a.half_height = hb["half_height"].get<float>();

      skeleton.hitboxes.push_back(std::move(a));
    }
    if (!skeleton.hitboxes.empty())
      std::cout << "Loaded " << skeleton.hitboxes.size()
                << " hitboxes for " << p.stem().string() << " ("
                << skeleton.joint_names.size() << " bones)\n";
  }
}

} // namespace

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

void add_animation_test_model(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &texture_cache) {
  const engine::LoadedGltfModel anim_model = engine::load_gltf_model(asset_path("/models/animationTest.glb"));
  if (anim_model.primitives.empty() || anim_model.skeletons.empty() || anim_model.animations.empty())
    return;

  add_gltf_model_instances(scene, texture_cache, anim_model, lifted(glm::vec3(0.0F, 1.5F, 0.0F)));

  engine::SkeletonAsset skeleton = anim_model.skeletons.front();
  load_model_metadata(asset_path("/models/animationTest.glb"), skeleton);

  const std::uint32_t skeleton_index = scene.add_skeleton(std::move(skeleton));

  for (const engine::AnimationClip &anim : anim_model.animations)
    (void)scene.add_animation(anim);

  const std::uint32_t first_instance =
      static_cast<std::uint32_t>(scene.instances().size() - anim_model.primitives.size());
  for (std::size_t i = 0; i < anim_model.primitives.size(); ++i)
    scene.instance(first_instance + static_cast<std::uint32_t>(i)).skin_index = skeleton_index;

  std::cout << "Loaded animation model with " << anim_model.animations.size()
            << " animations (" << anim_model.skeletons.front().joint_nodes.size() << " bones):";
  for (const engine::AnimationClip &anim : anim_model.animations)
    std::cout << ' ' << anim.name;
  std::cout << '\n';
}

void add_animation_test_model2(
    engine::Scene &scene,
    std::unordered_map<std::string, std::uint32_t> &texture_cache) {
  const engine::LoadedGltfModel model =
      engine::load_gltf_model(asset_path("/models/animationTest2.glb"));
  if (model.primitives.empty() || model.skeletons.empty() || model.animations.empty())
    return;

  add_gltf_model_instances(scene, texture_cache, model, lifted(glm::vec3(8.0F, 1.5F, 0.0F)));

  engine::SkeletonAsset skeleton = model.skeletons.front();
  load_model_metadata(asset_path("/models/animationTest2.glb"), skeleton);

  const std::uint32_t skeleton_index = scene.add_skeleton(std::move(skeleton));

  for (const engine::AnimationClip &anim : model.animations)
    (void)scene.add_animation(anim);

  const std::uint32_t first_instance =
      static_cast<std::uint32_t>(scene.instances().size() - model.primitives.size());
  for (std::size_t i = 0; i < model.primitives.size(); ++i)
    scene.instance(first_instance + static_cast<std::uint32_t>(i)).skin_index = skeleton_index;

  std::cout << "Loaded animation model 2 with " << model.animations.size()
            << " animations (" << model.skeletons.front().joint_nodes.size() << " bones):";
  for (const engine::AnimationClip &anim : model.animations)
    std::cout << ' ' << anim.name;
  std::cout << '\n';
}

TrimeshData load_trimesh_data() {
  const engine::LoadedGltfModel model = engine::load_gltf_model(asset_path("/models/trimeshTest.glb"));
  if (model.primitives.empty())
    return {};
  TrimeshData result;
  result.mesh.vertices = model.primitives.front().mesh.vertices;
  result.mesh.indices = model.primitives.front().mesh.indices;
  if (model.primitives.front().material.base_color_texture_path)
    result.texture_path = *model.primitives.front().material.base_color_texture_path;
  return result;
}

} // namespace app
