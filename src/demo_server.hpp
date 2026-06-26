#pragma once

#include "physics/physics_world.hpp"
#include "scene/animation_utils.hpp"
#include "scene/scene.hpp"

#include <SDL3/SDL_timer.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <vector>

class DemoServer {
public:
  explicit DemoServer(engine::Scene &scene, const std::vector<std::uint32_t> &cube_instances,
                      std::uint32_t character_instance, const engine::MeshSource *trimesh_source)
      : scene_{scene}, physics_(65536), character_instance_{character_instance} {
    if (trimesh_source && !trimesh_source->vertices.empty())
      (void)physics_.create_static_mesh(*trimesh_source, glm::vec3(0.0F));
    else
      (void)physics_.create_box({25.0F, 0.2F, 25.0F}, {0.0F, -0.2F, 0.0F},
                          JPH::EMotionType::Static, engine::physics::Layers::kNonMoving);

    for (std::uint32_t inst_idx : cube_instances) {
      const engine::MeshInstance &inst = scene_.instances()[inst_idx];
      const glm::vec3 pos{inst.model[3]};
      physics_bodies_.push_back({
          physics_.create_box({0.5F, 0.5F, 0.5F}, pos,
                              JPH::EMotionType::Dynamic, engine::physics::Layers::kMoving,
                              glm::quat(1.0F, 0.0F, 0.0F, 0.0F), 1.0F, 0.7F),
          inst_idx});
    }

    character_ = std::make_unique<engine::physics::Character>(physics_, glm::vec3{0.0F, 20.0F, 0.0F},
                                                                0.5F, 0.8F);

    std::cout << "Physics: " << physics_bodies_.size() << " cubes, "
              << (trimesh_source && !trimesh_source->vertices.empty() ? "trimesh ground" : "ground plane")
              << "\nCharacter at y=20\n";
  }

  [[nodiscard]] auto character_position() const -> glm::vec3 {
    const float elapsed = static_cast<float>(SDL_GetTicks() - render_state_.write_time_ms);
    const float fraction = std::clamp(elapsed / (1000.0F / 60.0F), 0.0F, 1.0F);
    return glm::mix(render_state_.prev, render_state_.curr, fraction);
  }

  void tick(float delta, float input_forward, float input_right, bool input_jump) {
    // Animation update
    for (engine::MeshInstance &instance : scene_.instances()) {
      if (instance.skin_index == engine::k_invalid_skin_index)
        continue;
      if (instance.animation_index >= scene_.animations().size())
        continue;

      const engine::AnimationClip &clip = scene_.animations()[instance.animation_index];
      instance.animation_time += delta * instance.animation_speed;
      if (instance.animation_loop && clip.duration > 0.0F && instance.animation_time > clip.duration)
        instance.animation_time = std::fmod(instance.animation_time, clip.duration);

      if (instance.next_animation_index < scene_.animations().size()) {
        const engine::AnimationClip &next_clip = scene_.animations()[instance.next_animation_index];
        instance.next_animation_time += delta * instance.animation_speed;
        if (instance.animation_loop && next_clip.duration > 0.0F &&
            instance.next_animation_time > next_clip.duration)
          instance.next_animation_time = std::fmod(instance.next_animation_time, next_clip.duration);

        instance.blend_factor += delta / instance.blend_duration;
        if (instance.blend_factor >= 1.0F) {
          instance.animation_index = instance.next_animation_index;
          instance.animation_time = instance.next_animation_time;
          instance.next_animation_index = std::numeric_limits<std::uint32_t>::max();
          instance.blend_factor = 1.0F;
        }
      }
    }

    // Compute character velocity from previous frame's state
    glm::vec3 vel = character_->linear_velocity();
    const bool grounded = character_->is_on_ground();

    const float accel = grounded ? 30.0F : 1.5F;
    vel.x += input_forward * accel * delta;
    vel.z += input_right * accel * delta;

    if (grounded)
      vel.y = input_jump ? 6.0F : 0.0F;

    float drag = grounded ? 8.0F : 0.5F;
    float t = 1.0F - std::exp(-drag * delta);
    vel.x = std::lerp(vel.x, 0.0F, t);
    vel.z = std::lerp(vel.z, 0.0F, t);

    character_->set_velocity(vel);
    character_->update(delta);

    physics_.step(delta);

    const glm::vec3 char_pos = character_->position();
    scene_.instance(character_instance_).model = glm::translate(glm::mat4(1.0F), char_pos);

    render_state_.prev = render_state_.curr;
    render_state_.curr = char_pos;
    render_state_.write_time_ms = SDL_GetTicks();

    for (const auto &pb : physics_bodies_)
      physics_.sync_body_to_instance(pb.body_id, scene_.instance(pb.instance_index));
  }

private:
  struct PhysicsEntry {
    JPH::BodyID body_id;
    std::uint32_t instance_index;
  };

  struct RenderState {
    glm::vec3 prev{0.0F, 20.0F, 0.0F};
    glm::vec3 curr{0.0F, 20.0F, 0.0F};
    std::uint64_t write_time_ms{};
  };

  engine::Scene &scene_;
  engine::physics::PhysicsWorld physics_;
  std::unique_ptr<engine::physics::Character> character_;
  std::uint32_t character_instance_{};
  RenderState render_state_;
  std::vector<PhysicsEntry> physics_bodies_;
};
