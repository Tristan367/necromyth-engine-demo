#include "demo_scene.hpp"

#include "demo_anim.hpp"
#include "demo_assets.hpp"
#include "demo_meshes.hpp"
#include "renderer/gltf_loader.hpp"
#include "scene/mesh_instance.hpp"
#include "scene/scene.hpp"
#include "scene/shadow_utils.hpp"
#include "scene_instance.hpp"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <unordered_map>


namespace app {

namespace {

constexpr std::size_t k_tile_array_layer_count = 6;

} // namespace

static std::vector<app::PhysicsObjDesc> obj_descs;
static std::uint32_t character_sphere_index;
static engine::MeshSource trimesh_mesh;

static void add_physics_test_objects(engine::Scene &scene);

void populate_demo_scene(engine::Scene &scene) {
  const DemoMeshLibrary assets = load_demo_mesh_library(scene);
  std::unordered_map<std::string, std::uint32_t> texture_cache;

  add_demo_sphere_instances(scene, texture_cache);

  scene.camera().look_at({4.0F, 3.0F, 6.0F}, {0.0F, 1.0F, 0.0F});
  scene.camera().set_perspective(45.0F, 0.05F, 2000.0F);

  scene.directional_light().direction_toward_light = glm::normalize(glm::vec3(0.4F, 1.0F, 0.3F));
  scene.directional_light().color = glm::vec3(1.0F, 0.98F, 0.92F);
  scene.directional_light().intensity = 1.0F;
  scene.directional_light().ambient = 0.18F;
  scene.shadow_settings() = engine::shadow_settings_from_environment();

  scene.point_lights().push_back({.position = {0.0F, 0.0F, 0.0F}, .color = {1.0F, 0.95F, 0.9F},
                                   .intensity = 3.0F, .range = 10.0F, .casts_shadow = true});
  // Stress test
  for (int i = 0; i < 9; ++i) {
    float angle = float(i) * std::numbers::pi_v<float> * 2.0F / 9.0f;
    float x = cos(angle) * 4.0f;
    float z = sin(angle) * 4.0f;
    scene.point_lights().push_back({.position = {x, 1.5f, z},
                                     .color = {0.6f + (i & 1) * 0.4f, 0.5f + (i & 2) * 0.25f, 0.5f + (i & 4) * 0.25f},
                                     .intensity = 2.0f, .range = 8.0f, .casts_shadow = true});
  }
  scene.spot_lights().push_back({.position = glm::vec3{0.0F, 2.0F, 0.0F}, .direction = glm::vec3{0.0F, 0.0F, -1.0F},
                                  .color = {1.0F, 0.95F, 0.7F}, .intensity = 1.5F, .range = 15.0F,
                                  .inner_angle = 0.12F, .outer_angle = 0.30F, .casts_shadow = true});

  (void)scene.add_instance({
      .mesh_index = assets.sky_mesh,
      .model = glm::scale(glm::mat4(1.0F), glm::vec3(500.0F)),
      .layer = engine::RenderLayer::Background,
  });

  (void)scene.add_instance({
      .mesh_index = assets.sky_mesh,
      .model = glm::scale(glm::mat4(1.0F), glm::vec3(500.0F)),
      .layer = engine::RenderLayer::Background,
  });

  const glm::mat4 susan_base = lifted(glm::vec3(-2.5F, 0.0F, -1.0F)) * glm::scale(glm::mat4(1.0F), glm::vec3(0.5F)) *
                               assets.susan_node_transform;
  demo_anim_add(
      SceneInstance(scene.add_instance({
          .mesh_index = assets.susan_gltf_mesh,
          .texture_index = assets.susan_gltf_texture,
          .texture_source = engine::TextureSource::Table,
          .model = susan_base,
          .layer = engine::RenderLayer::Opaque,
      })),
      susan_base,
      12.0F,
      glm::vec3(0.0F, 1.0F, 0.0F));

  const glm::mat4 wolf_base = lifted(glm::vec3(2.5F, 1.0F, -1.0F)) * glm::scale(glm::mat4(1.0F), glm::vec3(0.8F)) *
                              assets.wolf_node_transform;
  demo_anim_add(
      SceneInstance(scene.add_instance({
          .mesh_index = assets.wolf_mesh,
          .texture_index = assets.wolf_texture,
          .texture_source = engine::TextureSource::Table,
          .model = wolf_base,
          .layer = engine::RenderLayer::Opaque,
      })),
      wolf_base,
      -15.0F,
      glm::vec3(0.0F, 1.0F, 0.0F));

  const glm::mat4 suzanne_base = lifted(glm::vec3(-1.0F, 1.5F, 1.5F)) *
                                 glm::scale(glm::mat4(1.0F), glm::vec3(0.35F)) * assets.suzanne_node_transform;
  demo_anim_add(
      SceneInstance(scene.add_instance({
          .mesh_index = assets.suzanne_glb_mesh,
          .texture_index = assets.suzanne_glb_texture,
          .texture_source = engine::TextureSource::Table,
          .model = suzanne_base,
          .layer = engine::RenderLayer::Opaque,
      })),
      suzanne_base,
      18.0F,
      glm::vec3(0.0F, 1.0F, 0.0F));

  const glm::mat4 torus_base = lifted(glm::vec3(1.5F, 1.0F, 1.5F)) * glm::scale(glm::mat4(1.0F), glm::vec3(0.8F));
  demo_anim_add(
      SceneInstance(scene.add_instance({
          .mesh_index = assets.torus_mesh,
          .texture_index = assets.brick_array_layer,
          .texture_source = engine::TextureSource::ArrayLayer,
          .model = torus_base,
          .layer = engine::RenderLayer::Opaque,
      })),
      torus_base,
      24.0F,
      glm::vec3(1.0F, 0.0F, 0.0F));

  const glm::mat4 spin_torus_base =
      lifted(glm::vec3(0.0F, -0.5F, 0.0F)) * glm::scale(glm::mat4(1.0F), glm::vec3(1.1F));
  demo_anim_add(
      SceneInstance(scene.add_instance({
          .mesh_index = assets.torus_mesh,
          .texture_index = assets.dirt_table_texture,
          .texture_source = engine::TextureSource::Table,
          .model = spin_torus_base,
          .layer = engine::RenderLayer::Opaque,
      })),
      spin_torus_base,
      8.0F,
      glm::vec3(0.0F, 1.0F, 0.0F));

  constexpr int k_stress_ring_count = 16;
  for (int ring_index = 0; ring_index < k_stress_ring_count; ++ring_index) {
    const float angle =
        std::numbers::pi_v<float> * 2.0F * static_cast<float>(ring_index) / static_cast<float>(k_stress_ring_count);
    const glm::vec3 position{std::cos(angle) * 9.5F, 1.6F, std::sin(angle) * 9.5F};
    const std::uint32_t array_layer = static_cast<std::uint32_t>(ring_index % k_tile_array_layer_count);

    (void)scene.add_instance({
        .mesh_index = assets.torus_mesh,
        .texture_index = array_layer,
        .texture_source = engine::TextureSource::ArrayLayer,
        .model = glm::translate(glm::mat4(1.0F), position) * glm::scale(glm::mat4(1.0F), glm::vec3(0.55F)),
        .layer = engine::RenderLayer::Opaque,
    });
  }

  constexpr int k_stress_grid_side = 4;
  constexpr float k_stress_grid_spacing = 2.2F;
  const float grid_origin = (static_cast<float>(k_stress_grid_side) - 1.0F) * k_stress_grid_spacing * 0.5F;
  for (int row = 0; row < k_stress_grid_side; ++row) {
    for (int column = 0; column < k_stress_grid_side; ++column) {
      const glm::vec3 position{
          -grid_origin + static_cast<float>(column) * k_stress_grid_spacing,
          1.0F,
          6.0F + static_cast<float>(row) * k_stress_grid_spacing};

      const bool use_susan = (row + column) % 2 == 0;
      (void)scene.add_instance({
          .mesh_index = use_susan ? assets.susan_gltf_mesh : assets.suzanne_glb_mesh,
          .texture_index = use_susan ? assets.susan_gltf_texture : assets.suzanne_glb_texture,
          .texture_source = engine::TextureSource::Table,
          .model = glm::translate(glm::mat4(1.0F), position) *
                   glm::scale(glm::mat4(1.0F), glm::vec3(use_susan ? 0.35F : 0.28F)) *
                   (use_susan ? assets.susan_node_transform : assets.suzanne_node_transform),
          .layer = engine::RenderLayer::Opaque,
      });
    }
  }

  for (int wolf_index = 0; wolf_index < 6; ++wolf_index) {
    const float angle =
        std::numbers::pi_v<float> * 2.0F * static_cast<float>(wolf_index) / 6.0F + std::numbers::pi_v<float> / 6.0F;
    const glm::vec3 position{std::cos(angle) * 5.5F, 1.8F, std::sin(angle) * 5.5F - 3.0F};

    (void)scene.add_instance({
        .mesh_index = assets.wolf_mesh,
        .texture_index = assets.wolf_texture,
        .texture_source = engine::TextureSource::Table,
        .model = glm::translate(glm::mat4(1.0F), position) * glm::scale(glm::mat4(1.0F), glm::vec3(0.55F)) *
                 assets.wolf_node_transform,
        .layer = engine::RenderLayer::Opaque,
    });
  }

  const std::uint32_t alpha_test_texture = scene.add_texture(asset_path("/textures/alphaTest.png"));
  const std::uint32_t alpha_quad_mesh = scene.add_mesh(make_upright_quad_mesh(1.4F, 1.8F));

  (void)scene.add_instance({
      .mesh_index = alpha_quad_mesh,
      .texture_index = alpha_test_texture,
      .texture_source = engine::TextureSource::Table,
      .model = lifted(glm::translate(glm::mat4(1.0F), glm::vec3(-3.5F, 0.0F, 2.0F))),
      .layer = engine::RenderLayer::AlphaTested,
      .alpha_mode = engine::MeshAlphaMode::Cutout,
  });

  (void)scene.add_instance({
      .mesh_index = alpha_quad_mesh,
      .texture_index = alpha_test_texture,
      .texture_source = engine::TextureSource::Table,
      .model = lifted(glm::translate(glm::mat4(1.0F), glm::vec3(-1.8F, 0.0F, 2.0F))),
      .layer = engine::RenderLayer::AlphaTested,
      .alpha_mode = engine::MeshAlphaMode::AlphaToCoverage,
  });

  add_animation_test_model(scene, texture_cache);
  add_animation_test_model2(scene, texture_cache);

  // Load weapons and attach to model2's arm tips
  {
    const auto weapon_result = engine::import_gltf(scene, asset_path("/models/weaponTest.glb"),
                                                     lifted(glm::vec3(8.0F, 1.5F, 0.0F)));
    const auto weapon2_result = engine::import_gltf(scene, asset_path("/models/weaponTest.glb"),
                                                      lifted(glm::vec3(8.0F, 1.5F, 0.0F)));

    // Find model2's first instance and attach weapons to arm tips
    for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(scene.instances().size()); ++i) {
      const engine::MeshInstance &inst = scene.instances()[i];
      if (inst.skin_index != engine::k_invalid_skin_index && inst.skin_index > 0) {
        if (weapon_result.first_instance != 0)
          scene.instance(i).bone_attachments.push_back(
              engine::BoneAttachment{.joint_index = 9, .target_instance = weapon_result.first_instance});
        if (weapon2_result.first_instance != 0)
          scene.instance(i).bone_attachments.push_back(
              engine::BoneAttachment{.joint_index = 5, .target_instance = weapon2_result.first_instance});
        break;
      }
    }
  }

  add_physics_test_objects(scene);

  // Character visual (capsule that will follow character position)
  {
    const engine::MeshSource vis_mesh = app::make_capsule_mesh(0.5F, 0.4F, 8, 4);
    const std::uint32_t vis_mesh_idx = scene.add_mesh(vis_mesh);
    character_sphere_index = scene.add_instance({
        .mesh_index = vis_mesh_idx,
        .texture_index = 0,
        .model = glm::mat4(1.0F),
        .layer = engine::RenderLayer::Opaque,
    });
  }

  // Load trimesh terrain
  {
    const TrimeshData terrain = load_trimesh_data();
    if (!terrain.mesh.vertices.empty()) {
      trimesh_mesh = terrain.mesh;
      const std::uint32_t mesh_idx = scene.add_mesh(trimesh_mesh);
      std::uint32_t tex_idx = 0;
      if (!terrain.texture_path.empty())
        tex_idx = scene.add_texture(terrain.texture_path);
      (void)scene.add_instance({
          .mesh_index = mesh_idx,
          .texture_index = tex_idx,
          .model = glm::translate(glm::mat4(1.0F), glm::vec3(0.0F, -3.0F, 0.0F)),
          .layer = engine::RenderLayer::Opaque,
      });
      std::cout << "Loaded trimesh terrain: " << terrain.mesh.vertices.size() << " verts\n";
    }
  }
}

auto create_demo_scene(
    std::vector<app::PhysicsObjDesc> *out_obj_descs,
    std::uint32_t *out_char_instance,
    engine::MeshSource *out_trimesh_mesh) -> engine::Scene {
  engine::Scene scene;
  populate_demo_scene(scene);
  if (out_obj_descs)
    *out_obj_descs = std::move(obj_descs);
  if (out_char_instance)
    *out_char_instance = character_sphere_index;
  if (out_trimesh_mesh)
    *out_trimesh_mesh = std::move(trimesh_mesh);
  obj_descs.clear();
  character_sphere_index = 0;
  trimesh_mesh = {};
  return scene;
}

void update_demo_scene(engine::Scene &scene) {
  update_demo_animations(scene);
}

void add_physics_test_objects(engine::Scene &scene) {
  struct ObjectDef {
    engine::MeshSource (*maker)();
    glm::vec3 pos;
    glm::vec3 box_half_extent;
  };

  struct { float r, g, b; } colors[] = {
    {0.8F, 0.2F, 0.2F}, {0.2F, 0.8F, 0.2F}, {0.2F, 0.2F, 0.8F}, {0.8F, 0.8F, 0.2F},
    {0.8F, 0.2F, 0.8F}, {0.2F, 0.8F, 0.8F}, {0.5F, 0.3F, 0.7F}, {0.3F, 0.7F, 0.5F},
    {0.7F, 0.5F, 0.3F}, {0.9F, 0.6F, 0.1F}, {0.1F, 0.6F, 0.9F}, {0.6F, 0.9F, 0.1F},
    {0.4F, 0.4F, 0.8F}, {0.4F, 0.8F, 0.4F}, {0.8F, 0.4F, 0.4F}, {0.5F, 0.5F, 0.5F},
    {1.0F, 0.3F, 0.3F}, {0.3F, 1.0F, 0.3F}, {0.3F, 0.3F, 1.0F}, {1.0F, 0.7F, 0.2F},
  };

  std::srand(42);

  const std::uint32_t white_tex = scene.add_texture(asset_path("/textures/white.png"));

  for (int i = 0; i < 50; ++i) {
    auto &c = colors[i % 20];
    const float w = c.r, v = c.g, b = c.b;

    const int shape = std::rand() % 6;
    const float s = 0.05F + static_cast<float>(std::rand() % 1000) * 0.001F;  // 0.05 - 1.05
    const float sx = s * (0.3F + static_cast<float>(std::rand() % 300) * 0.003F);
    const float sy = s * (0.3F + static_cast<float>(std::rand() % 300) * 0.003F);
    const float sz = s * (0.3F + static_cast<float>(std::rand() % 300) * 0.003F);
    const float x = static_cast<float>(std::rand() % 1000) * 0.02F - 10.0F;
    const float y = 20.0F + static_cast<float>(std::rand() % 2000) * 0.02F;
    const float z = static_cast<float>(std::rand() % 1000) * 0.02F - 10.0F;

    engine::MeshSource mesh;
    app::PhysicsObjDesc desc;

    switch (shape) {
    case 0: mesh = app::make_box_mesh(sx, sy, sz);
            desc = {0, app::TestObjShape::Box, glm::vec3(sx, sy, sz)}; break;
    case 1: mesh = app::make_sphere_mesh(s);
            desc = {0, app::TestObjShape::Sphere, glm::vec3(s, 0, 0), s}; break;
    case 2: mesh = app::make_capsule_mesh(s, s * 1.5F);
            desc = {0, app::TestObjShape::Capsule, glm::vec3(s * 1.5F, 0, 0), s}; break;
    case 3: mesh = app::make_capsule_mesh(s, s * 0.7F, s * 1.2F);
            desc = {0, app::TestObjShape::TaperedCapsule, glm::vec3(s * 1.2F, 0, 0), s, s * 0.7F}; break;
    case 4: mesh = app::make_cylinder_mesh(s, s, s * 1.2F);
            desc = {0, app::TestObjShape::Cylinder, glm::vec3(s * 1.2F, 0, 0), s}; break;
    case 5: mesh = app::make_cylinder_mesh(s, s * 0.6F, s * 1.5F);
            desc = {0, app::TestObjShape::TaperedCylinder, glm::vec3(s * 1.5F, 0, 0), s, s * 0.6F}; break;
    }

    for (auto &vert : mesh.vertices) {
      vert.color[0] = w; vert.color[1] = v; vert.color[2] = b;
    }

    desc.instance_index = scene.add_instance({
        .mesh_index = scene.add_mesh(mesh),
        .texture_index = white_tex,
        .model = glm::translate(glm::mat4(1.0F), glm::vec3{x, y, z}),
        .layer = engine::RenderLayer::Opaque,
    });
    obj_descs.push_back(desc);
  }
}

} // namespace app

